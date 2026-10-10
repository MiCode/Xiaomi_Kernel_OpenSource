#ifndef __LEDS_SUBPMIC_FLASH_H
#define __LEDS_SUBPMIC_FLASH_H

#include <linux/leds.h>
#include <linux/led-class-flash.h>

#define ENABLE_REGULATOR           BIT(0)
#define DISABLE_REGULATOR          BIT(1)
#define QUERY_MAX_AVAIL_CURRENT    BIT(2)

/**
 * struct flash_led_param: subpmic flash LED parameter data
 * @on_time_ms	: Time to wait before strobing the switch
 * @off_time_ms	: Time to wait to turn off LED after strobing switch
 */
struct flash_led_param {
	u64 on_time_ms;
	u64 off_time_ms;
};

enum flashlight_type{
	FLASHLIGHT_TYPE_FLASH = 1,
	FLASHLIGHT_TYPE_TORCH = 2,
};

enum flashlight_id {
	FLASHLIGHT_LED1 = BIT(0),
	FLASHLIGHT_LED2 = BIT(1),
	FLASHLIGHT_LED1_2 = (FLASHLIGHT_LED1|FLASHLIGHT_LED2),
};

/**
 * struct subpmic_flash_led: Main Flash LED data structure
 * @pdev:                 Pointer for platform device
 * @fnode:                Pointer for array of child LED devices
 * @snode:                Pointer for array of child switch devices
 * @lock:                 Spinlock to be used for critical section
 * @num_fnodes:           Number of flash/torch nodes defined in device tree
 * @num_snodes:           Number of switch nodes defined in device tree
 * @all_ramp_up_done_irq: IRQ number for all ramp up interrupt
 * @max_current:          Maximum current available for flash
 * @max_channels:         Maximum number of channels supported by flash module
 * @module_en:            Flag used to enable/disable flash LED module
 */
struct subpmic_flash_led {
	void                            *private;
	struct subpmic_led_flash_ops    *ops;
	struct platform_device          *pdev;
	struct flash_node_data          *fnode;
	struct flash_switch_data        *snode;
	spinlock_t                      lock;
	u32                             num_fnodes;
	u32                             num_snodes;
	int                             max_current;
	u8                              max_channels;
	u8                              last_configure;
};

struct flash_node_data {
	struct subpmic_flash_led      *led;
	struct led_classdev_flash     fdev;
	u32                           user_current_ma;
	u32                           current_ma;
	u32                           max_current;
	u8                            duration;
	u8                            id;
	enum flashlight_type          type;
	bool                          enabled;
};

struct subpmic_led_flash_ops {
	/* set flash brightness */
	int (*brightness_set)(void * private, enum flashlight_type type, int index, int brightness);
	/* get flash brightness */
	int (*brightness_get)(void * private, enum flashlight_type type, int index, int *brightness);
	/* set flash strobe state */
	int (*strobe_set)(void * private, enum flashlight_type type, int index, bool state);
	/* get flash strobe state */
	int (*strobe_get)(void * private, enum flashlight_type type, int index, bool *state);
	/* set flash timeout */
	int (*flash_timeout_set)(void * private, int timeout);
	/* get the flash LED fault */
	//int (*fault_get)(struct led_classdev_flash *fled_cdev, int *fault);
};

#if IS_ENABLED(CONFIG_LEDS_SUBPMIC_FLASH)
int subpmic_flash_led_prepare(struct led_trigger *trig,
			int options, int *max_current);
int subpmic_flash_led_set_param(struct led_trigger *trig,
			struct flash_led_param param);
#else
static inline int subpmic_flash_led_prepare(struct led_trigger *trig,
					int options, int *max_current)
{
	return -EINVAL;
}

static inline int subpmic_flash_led_set_param(struct led_trigger *trig,
			struct flash_led_param param)
{
	return -EINVAL;
}
#endif
#endif

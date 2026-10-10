#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/leds-subpmic-flash.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/soc/qcom/battery_charger.h>
#include "leds.h"

#define  PROBE_CNT_MAX                         50
#define  FLASH_LED_SAFETY_TIMER_EN             BIT(7)
#define  SAFETY_TIMER_MAX_TIMEOUT_MS           400
#define  SAFETY_TIMER_MIN_TIMEOUT_MS           10
#define  SAFETY_TIMER_STEP_SIZE                10
#define  SAFETY_TIMER_DEFAULT_TIMEOUT_MS       200
#define  DEFAULT_CURR_MA                       25

#define  CONFIG_TYPE_SHIFT 4
#define  CONFIG_TYPE_MASK 0xf0
#define  CONFIG_ID_MASK 0x0f
#define  GET_CONFIG(type, id)   \
	(((type << CONFIG_TYPE_SHIFT) & CONFIG_TYPE_MASK) | (id & CONFIG_ID_MASK))
#define  GET_TYPE(config) ((config&CONFIG_TYPE_MASK)>>CONFIG_TYPE_SHIFT)
#define  GET_ID(config) (config&CONFIG_ID_MASK)
#define  GET_HANDLE_RETRY_TIMES 3

struct flash_switch_data {
	struct subpmic_flash_led     *led;
	struct led_classdev          cdev;
	struct hrtimer               on_timer;
	struct hrtimer               off_timer;
	u64                          on_time_ms;
	u64                          off_time_ms;
	u32                          id;
	bool                         enabled;
};

extern  void sc6601_fill_leds_data(struct subpmic_flash_led* led);
extern  void nu6601_fill_leds_data(struct subpmic_flash_led* led);

static bool is_subpmic_available(struct subpmic_flash_led *led) {
	int i = 0;
	while(i < GET_HANDLE_RETRY_TIMES) {
		if(NULL != led->ops && NULL != led->private) {
			return true;
		}
		sc6601_fill_leds_data(led);
		//set_sc6601_leds_data(led);
		i++;
		pr_err("get subpmic handle  %d times. private:%p, ops:%p\n",
					i+1, led->private, led->ops);
	}
	return false;
}

static int get_fnode_index(struct subpmic_flash_led *led,
			enum flashlight_type type, enum flashlight_id id) {
	int i = 0;
	for (; i < led->num_fnodes; i++) {
		if(type == led->fnode[i].type && id == led->fnode[i].id) {
			return i;
		}
	}
	return -1;
}

static bool is_other_led_configured(struct flash_node_data *fnode)
{
	struct subpmic_flash_led *led = fnode->led;
	int index = -1;
	if(0 == led->last_configure ||
		GET_CONFIG(fnode->type, fnode->id) == led->last_configure) {
		return false;
	}
	index = get_fnode_index(led,
		GET_TYPE(led->last_configure), GET_ID(led->last_configure));
	if(-1 == index) {
		led->last_configure = 0;
		return false;
	}
	if(led->fnode[index].enabled) {
		return true;
	} else {
		led->last_configure &= ~GET_CONFIG(led->fnode[index].type, led->fnode[index].id);
	}

	return false;
}

static int subpmic_flash_led_strobe(struct subpmic_flash_led *led,
			struct flash_switch_data *snode, bool enable)
{
	int rc = 0;
	int index = -1;

	if (snode && snode->off_time_ms) {
		pr_debug("Off timer started with delay %d ms\n",
			snode->off_time_ms);
		hrtimer_start(&snode->off_timer,
			ms_to_ktime(snode->off_time_ms),
			HRTIMER_MODE_REL);
		return rc;
	}
	if(is_subpmic_available(led)) {
		rc = led->ops->strobe_set(led->private, GET_TYPE(led->last_configure),
					GET_ID(led->last_configure), enable);
		if (rc < 0) {
			pr_err("fail to set snode(%d) %s last_configure:0x%x rc=%d\n",
						snode->id, enable?"enable":"disable",
						led->last_configure, rc);
		} else {
			//spin_lock(&led->lock);
			index = get_fnode_index(led, GET_TYPE(led->last_configure),
						GET_ID(led->last_configure));
			if(-1 != index) {
				led->fnode[index].enabled = enable;
			}
			//spin_unlock(&led->lock);
		}
	} else {
		rc =  -EINVAL;
	}

	return rc;
}

static int subpmic_flash_led_enable(struct flash_node_data *fnode)
{
	struct subpmic_flash_led *led = fnode->led;
	int rc = 0;

	if(is_subpmic_available(led)) {
		rc = led->ops->brightness_set(led->private, fnode->type,
					fnode->id, fnode->current_ma);
		if (rc < 0) {
			pr_err("fail to set fnode(%d-%d) brightness:%d\n",
						fnode->type, fnode->id, fnode->current_ma);
			goto out;
		}
	} else {
		rc =  -EINVAL;
		goto out;
	}

	/*
	 * For dynamic brightness control of Torch LEDs,
	 * just configure the target current.
	 */
	if (fnode->type == FLASHLIGHT_TYPE_TORCH && fnode->enabled) {
		return 0;
	}

	if (fnode->type == FLASHLIGHT_TYPE_FLASH) {
		if(is_subpmic_available(led)) {
			rc = led->ops->flash_timeout_set(led->private, fnode->duration);
			if (rc < 0) {
				pr_err("fail to set fnode(%d-%d) timeout:%d\n",
							fnode->type, fnode->id, fnode->duration);
				goto out;
			}
		} else {
			rc =  -EINVAL;
			goto out;
		}
	}
	//spin_lock(&led->lock);
	led->last_configure |= GET_CONFIG(fnode->type, fnode->id);
	//spin_unlock(&led->lock);

out:
	return rc;
}

static int subpmic_flash_led_disable(struct flash_node_data *fnode)
{
	struct subpmic_flash_led *led = fnode->led;
	int rc = 0;

	if(led->last_configure != GET_CONFIG(fnode->type, fnode->id)) {
		pr_err("0x%x is not configured\n", led->last_configure);
		return 0;
	}

	if(is_subpmic_available(led)) {
		rc = led->ops->brightness_set(led->private, fnode->type, fnode->id, 0);
		if (rc < 0){
			pr_err("fail to set fnode(%d-%d) brightness:0\n", fnode->type, fnode->id);
			goto out;
		}
	} else {
		rc =  -EINVAL;
		goto out;
	}

	//spin_lock(&led->lock);
	led->last_configure &= ~GET_CONFIG(fnode->type, fnode->id);
	fnode->current_ma = 0;
	fnode->user_current_ma = 0;
	//spin_unlock(&led->lock);

out:
	return rc;
}

static enum led_brightness subpmic_flash_led_brightness_get(
						struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static void subpmic_flash_led_brightness_set(struct led_classdev *led_cdev,
					enum led_brightness brightness)
{
	struct led_classdev_flash *fdev;
	struct flash_node_data *fnode;
	struct subpmic_flash_led *led;
	int rc;

	fdev = container_of(led_cdev, struct led_classdev_flash, led_cdev);
	fnode = container_of(fdev, struct flash_node_data, fdev);
	led = fnode->led;
	if (is_other_led_configured(fnode)) {
		pr_err("fnode(%d, %d) is configured", GET_TYPE(led->last_configure),
					GET_ID(led->last_configure));
		return;
	}

	if (!brightness) {
		rc = subpmic_flash_led_strobe(fnode->led, NULL, false);
		if (rc < 0) {
			pr_err("Failed to destrobe LED, rc=%d\n", rc);
			return;
		}

		rc = subpmic_flash_led_disable(fnode);
		if (rc < 0)
			pr_err("Failed to disable LED\n");

		led_cdev->brightness = 0;

		return;
	}

	fnode->current_ma = min(brightness, fnode->max_current);
	led_cdev->brightness = fnode->current_ma ;

	rc = subpmic_flash_led_enable(fnode);
	if (rc < 0)
		pr_err("Failed to set brightness %d to LED\n", brightness);

	if (!rc)
		fnode->user_current_ma = brightness;
	else
		return;
}

static int subpmic_flash_switch_enable(struct flash_switch_data *snode)
{
	struct subpmic_flash_led *led = snode->led;

	if(GET_ID(led->last_configure) != snode->id) {
		/*
		 * Do not turn ON flash/torch device if
		 * i. the device is not under this switch or
		 * ii. brightness is not configured for device under this switch
		 */
		pr_err("switch node(id:%d) does not compare whit configured fnode(id:%d).",
					snode->id, GET_ID(led->last_configure));
		return 0;
	}

	return subpmic_flash_led_strobe(led, snode, true);
}

static int subpmic_flash_switch_disable(struct flash_switch_data *snode)
{
	struct subpmic_flash_led *led = snode->led;
	int rc = 0, i;

	if((GET_ID(led->last_configure)) != snode->id) {
		pr_err("switch node(id:%d) does not compare whit configured fnode(id:%d).",
					snode->id, GET_ID(led->last_configure));
		return 0;
	}
	rc = subpmic_flash_led_strobe(led, NULL, false);
	if (rc < 0) {
		pr_err("Failed to destrobe LEDs under with switch, rc=%d\n",
			rc);
		return rc;
	}

	for (i = 0; i < led->num_fnodes; i++) {
		/*
		 * Do not turn OFF flash/torch device if
		 * i. the device is not under this switch or
		 * ii. brightness is not configured for device under this switch
		 */
		if(led->last_configure != GET_CONFIG(led->fnode[i].type, led->fnode[i].id)) {
			continue;
		}

		rc = subpmic_flash_led_disable(&led->fnode[i]);
		if (rc < 0) {
			pr_err("Failed to disable LED%d\n",
				&led->fnode[i].id);
			break;
		}
	}

	snode->on_time_ms = 0;
	snode->off_time_ms = 0;

	return rc;
}

static void subpmic_flash_led_switch_brightness_set(
		struct led_classdev *led_cdev, enum led_brightness value)
{
	struct flash_switch_data *snode = NULL;
	int rc = 0;
	bool state = value > 0;

	snode = container_of(led_cdev, struct flash_switch_data, cdev);

	if (snode->enabled == state) {
		pr_debug("Switch  is already %s!\n",
			state ? "enabled" : "disabled");
		return;
	}

	if (state) {
		if (snode->on_time_ms) {
			pr_debug("On timer started with delay %d ms\n",
				snode->on_time_ms);
			hrtimer_start(&snode->on_timer,
					ms_to_ktime(snode->on_time_ms),
					HRTIMER_MODE_REL);
			snode->enabled = state;
			return;
		}

		rc = subpmic_flash_switch_enable(snode);
	} else {
		rc = subpmic_flash_switch_disable(snode);
	}

	if (rc < 0)
		pr_err("Failed to %s switch, rc=%d\n",
			state ? "enable" : "disable", rc);
	else
		snode->enabled = state;
}

static struct led_classdev *trigger_to_lcdev(struct led_trigger *trig)
{
	struct led_classdev *led_cdev;

	read_lock(&trig->leddev_list_lock);
	list_for_each_entry(led_cdev, &trig->led_cdevs, trig_list) {
		if (!strcmp(led_cdev->default_trigger, trig->name)) {
			read_unlock(&trig->leddev_list_lock);
			return led_cdev;
		}
	}

	read_unlock(&trig->leddev_list_lock);
	return NULL;
}

static enum hrtimer_restart off_timer_function(struct hrtimer *timer)
{
	struct flash_switch_data *snode = container_of(timer,
			struct flash_switch_data, off_timer);
	int rc = 0;
	pr_err("off_timer_function E\n");

	rc = subpmic_flash_switch_disable(snode);
	if (rc < 0)
		pr_err("Failed to disable flash LED switch %s, rc=%d\n",
			snode->cdev.name, rc);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart on_timer_function(struct hrtimer *timer)
{
	struct flash_switch_data *snode = container_of(timer,
			struct flash_switch_data, on_timer);
	int rc = 0;

	pr_err("on_timer_function E\n");
	rc = subpmic_flash_switch_enable(snode);
	if (rc < 0) {
		snode->enabled = false;
		pr_err("Failed to enable flash LED switch %s, rc=%d\n",
			snode->cdev.name, rc);
	}

	return HRTIMER_NORESTART;
}

int subpmic_flash_led_set_param(struct led_trigger *trig,
					struct flash_led_param param)
{
	struct led_classdev *led_cdev = trigger_to_lcdev(trig);
	struct flash_switch_data *snode;

	if (!led_cdev) {
		pr_err("Invalid led_cdev in trigger %s\n", trig->name);
		return -EINVAL;
	}

	if (!param.on_time_ms && !param.off_time_ms) {
		pr_err("Invalid param, on_time/off_time cannot be 0\n");
		return -EINVAL;
	}

	snode = container_of(led_cdev, struct flash_switch_data, cdev);
	snode->on_time_ms = param.on_time_ms;
	snode->off_time_ms = param.off_time_ms;

	return 0;
}
EXPORT_SYMBOL(subpmic_flash_led_set_param);

int subpmic_flash_led_prepare(struct led_trigger *trig, int options,
				int *max_current)
{
	struct led_classdev *led_cdev;
	struct flash_switch_data *snode;
	pr_err("subpmic_flash_led_prepare E  options:0x%x\n", options);

	if (!trig) {
		pr_err("Invalid led_trigger\n");
		return -EINVAL;
	}

	led_cdev = trigger_to_lcdev(trig);
	if (!led_cdev) {
		pr_err("Invalid led_cdev in trigger %s\n", trig->name);
		return -ENODEV;
	}

	snode = container_of(led_cdev, struct flash_switch_data, cdev);

	if (options & QUERY_MAX_AVAIL_CURRENT) {
		if (!max_current) {
			pr_err("Invalid max_current pointer\n");
			return -EINVAL;
		}

		snode->led->max_current = *max_current;
	}

	return 0;
}
EXPORT_SYMBOL(subpmic_flash_led_prepare);

static ssize_t subpmic_flash_led_max_current_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct flash_switch_data *snode;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	snode = container_of(led_cdev, struct flash_switch_data, cdev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", snode->led->max_current);
}

static ssize_t subpmic_flash_on_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct flash_switch_data *snode;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	u64 val;

	rc = kstrtou64(buf, 0, &val);
	if (rc < 0)
		return rc;

	if (!val)
		return -EINVAL;

	snode = container_of(led_cdev, struct flash_switch_data, cdev);
	snode->on_time_ms = val;

	return count;
}

static ssize_t subpmic_flash_on_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct flash_switch_data *snode;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	snode = container_of(led_cdev, struct flash_switch_data, cdev);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", snode->on_time_ms * 1000);
}

static ssize_t subpmic_flash_off_time_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	struct flash_switch_data *snode;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	u64 val;

	rc = kstrtou64(buf, 0, &val);
	if (rc < 0)
		return rc;

	val = min_t(u64, val, SAFETY_TIMER_MAX_TIMEOUT_MS);

	snode = container_of(led_cdev, struct flash_switch_data, cdev);
	snode->off_time_ms = val;

	return count;
}

static ssize_t subpmic_flash_off_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct flash_switch_data *snode;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	snode = container_of(led_cdev, struct flash_switch_data, cdev);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", snode->off_time_ms * 1000);
}

static struct device_attribute subpmic_flash_led_attrs[] = {
	__ATTR(max_current, 0400, subpmic_flash_led_max_current_show, NULL),
	__ATTR(on_time, 0600, subpmic_flash_on_time_show,
		subpmic_flash_on_time_store),
	__ATTR(off_time, 0600, subpmic_flash_off_time_show,
		subpmic_flash_off_time_store),
};

static int subpmic_flash_brightness_set_blocking(
		struct led_classdev *led_cdev, enum led_brightness value)
{
	subpmic_flash_led_brightness_set(led_cdev, value);

	return 0;
}

static int subpmic_flash_brightness_set(
		struct led_classdev_flash *fdev, u32 brightness)
{
	subpmic_flash_led_brightness_set(&fdev->led_cdev, brightness);

	return 0;
}

static int subpmic_flash_brightness_get(
		struct led_classdev_flash *fdev, u32 *brightness)
{
	*brightness = subpmic_flash_led_brightness_get(&fdev->led_cdev);

	return 0;
}

static int subpmic_flash_strobe_set(struct led_classdev_flash *fdev,
				bool state)
{
	struct flash_node_data *fnode;
	int rc;

	fnode = container_of(fdev, struct flash_node_data, fdev);

	if (fnode->enabled == state)
		return 0;

	if(fnode->led->last_configure != GET_CONFIG(fnode->type, fnode->id)) {
		return -EINVAL;
	}

	if (!fnode->duration) {
		pr_debug("Safety time duration is zero, strobe not set\n");
		return -EINVAL;
	}

	rc = subpmic_flash_led_strobe(fnode->led, NULL, state);
	if (rc < 0) {
		pr_err("Failed to %s LED, rc=%d\n",
			state ? "strobe" : "desrobe", rc);
		return rc;
	}
	fnode->enabled = state;

	if (!state) {
		rc = subpmic_flash_led_disable(fnode);
		if (rc < 0)
			pr_err("Failed to disable LED %u\n", fnode->id);
	}

	return rc;
}

static int subpmic_flash_strobe_get(struct led_classdev_flash *fdev,
				bool *state)
{
	struct flash_node_data *fnode = container_of(fdev,
			struct flash_node_data, fdev);

	*state = fnode->enabled;

	return 0;
}

static int subpmic_flash_timeout_set(struct led_classdev_flash *fdev,
				u32 timeout)
{
	struct subpmic_flash_led *led;
	struct flash_node_data *fnode;
	int rc = 0;

	fnode = container_of(fdev, struct flash_node_data, fdev);
	led = fnode->led;

	if (!timeout) {
		fnode->duration = 0;
		return 0;
	}

	fnode->duration = timeout / 1000;

	if(is_subpmic_available(led)) {
		rc = led->ops->flash_timeout_set(led->private, fnode->duration);
		if(rc < 0) {
			pr_err("subpmic flas set timeout error. rc=%d", rc);
		}
	} else {
		return -EINVAL;
	}

	return rc;
}

static const struct led_flash_ops flash_ops = {
	.flash_brightness_set    = subpmic_flash_brightness_set,
	.flash_brightness_get    = subpmic_flash_brightness_get,
	.strobe_set              = subpmic_flash_strobe_set,
	.strobe_get              = subpmic_flash_strobe_get,
	.timeout_set             = subpmic_flash_timeout_set,
};

static int subpmic_flash_led_setup(struct subpmic_flash_led *led)
{
	int rc = 0, i, addr_offset;

	for (i = 0; i < led->num_fnodes; i++) {
		addr_offset = led->fnode[i].id;
	}
	return rc;
}

static int register_switch_device(struct subpmic_flash_led *led,
			struct flash_switch_data *snode, struct device_node *node)
{
	int rc, i;
	const char *temp_string;

	rc = of_property_read_string(node, "qcom,led-name",
				&snode->cdev.name);
	if (rc < 0) {
		pr_err("Failed to read switch node name, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
					&snode->cdev.default_trigger);
	if (rc < 0) {
		pr_err("Failed to read trigger name, rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,leds", &temp_string);
	if (rc < 0) {
		pr_err("Failed to read switch qcom,leds\n");
		return rc;
	}
	if (!strcmp(temp_string, "led1")) {
		snode->id = FLASHLIGHT_LED1;
	} else if (!strcmp(temp_string, "led2")) {
		snode->id = FLASHLIGHT_LED2;
	} else {
		pr_err("Incorrect flash LED id %s\n", temp_string);
		return rc;
	}

	snode->on_time_ms = 0;
	snode->off_time_ms = 0;
	hrtimer_init(&snode->on_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hrtimer_init(&snode->off_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	snode->on_timer.function = on_timer_function;
	snode->off_timer.function = off_timer_function;

	snode->led = led;
	snode->cdev.brightness_set = subpmic_flash_led_switch_brightness_set;
	snode->cdev.brightness_get = subpmic_flash_led_brightness_get;

	rc = devm_led_classdev_register(&led->pdev->dev, &snode->cdev);
	if (rc < 0) {
		pr_err("Failed to register led switch device:%s\n",
			snode->cdev.name);
		return rc;
	}

	for (i = 0; i < ARRAY_SIZE(subpmic_flash_led_attrs); i++) {
		rc = sysfs_create_file(&snode->cdev.dev->kobj,
					&subpmic_flash_led_attrs[i].attr);
		if (rc < 0) {
			pr_err("Failed to create sysfs attrs, rc=%d\n", rc);
			goto sysfs_fail;
		}
	}

	return 0;

sysfs_fail:
	while (i >= 0)
		sysfs_remove_file(&snode->cdev.dev->kobj,
			&subpmic_flash_led_attrs[i--].attr);
	return rc;
}

static int register_flash_device(struct subpmic_flash_led *led,
			struct flash_node_data *fnode, struct device_node *node)
{
	struct led_flash_setting *setting;
	const char *temp_string;
	int rc;
	u32 val, default_curr_ma;

	rc = of_property_read_string(node, "qcom,led-name",
				&fnode->fdev.led_cdev.name);
	if (rc < 0) {
		pr_err("Failed to read flash LED names\n");
		return rc;
	}

	rc = of_property_read_string(node, "qcom,type", &temp_string);
	if (rc < 0) {
		pr_err("Failed to read flash LED type\n");
		return rc;
	}

	if (!strcmp(temp_string, "flash")) {
		fnode->type = FLASHLIGHT_TYPE_FLASH;
	} else if (!strcmp(temp_string, "torch")) {
		fnode->type = FLASHLIGHT_TYPE_TORCH;
	} else {
		pr_err("Incorrect flash LED type %s\n", temp_string);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,leds", &temp_string);
	if (rc < 0) {
		pr_err("Failed to read flash qcom,leds\n");
		return rc;
	}
	if (!strcmp(temp_string, "led1")) {
		fnode->id = FLASHLIGHT_LED1;
	} else if (!strcmp(temp_string, "led2")) {
		fnode->id = FLASHLIGHT_LED2;
	} else {
		pr_err("Incorrect flash LED id %s\n", temp_string);
		return rc;
	}

	rc = of_property_read_string(node, "qcom,default-led-trigger",
				&fnode->fdev.led_cdev.default_trigger);
	if (rc < 0) {
		pr_err("Failed to read trigger name\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,max-current-ma",
				&fnode->max_current);
	if (rc < 0) {
		pr_err("Failed to read max current, rc=%d\n", rc);
		return rc;
	}

	fnode->fdev.led_cdev.max_brightness = fnode->max_current;

	fnode->duration = SAFETY_TIMER_DEFAULT_TIMEOUT_MS;
	rc = of_property_read_u32(node, "qcom,duration-ms", &val);
	if (!rc && (val >= SAFETY_TIMER_MIN_TIMEOUT_MS &&
				val <= SAFETY_TIMER_MAX_TIMEOUT_MS))
		fnode->duration = val;

	fnode->led = led;
	fnode->fdev.led_cdev.brightness_set = subpmic_flash_led_brightness_set;
	fnode->fdev.led_cdev.brightness_get = subpmic_flash_led_brightness_get;
	fnode->enabled = false;
	fnode->fdev.ops = &flash_ops;

	if (fnode->type == FLASHLIGHT_TYPE_FLASH) {
		fnode->fdev.led_cdev.flags = LED_DEV_CAP_FLASH;
		fnode->fdev.led_cdev.brightness_set_blocking =
					subpmic_flash_brightness_set_blocking;
	}

	default_curr_ma = DEFAULT_CURR_MA;
	setting = &fnode->fdev.brightness;
	setting->min = 0;
	setting->max = fnode->max_current;
	setting->step = 1;
	setting->val = default_curr_ma;

	setting = &fnode->fdev.timeout;
	setting->min = 0;
	setting->max = SAFETY_TIMER_MAX_TIMEOUT_MS;
	setting->step = SAFETY_TIMER_STEP_SIZE;
	setting->val = SAFETY_TIMER_DEFAULT_TIMEOUT_MS;

	rc = led_classdev_flash_register(&led->pdev->dev, &fnode->fdev);
	if (rc < 0) {
		pr_err("Failed to register flash led device:%s\n",
					fnode->fdev.led_cdev.name);
		return rc;
	}

	return 0;
}

static int subpmic_flash_led_register_device(
			struct subpmic_flash_led *led,
			struct device_node *node)
{
	struct device_node *temp;
	const char *type;
	int rc, i = 0, j = 0;

	for_each_available_child_of_node(node, temp) {
		rc = of_property_read_string(temp, "qcom,type", &type);
		if (rc < 0) {
			pr_err("Failed to parse type, rc=%d\n", rc);
			of_node_put(temp);
			return rc;
		}

		if (!strcmp("flash", type) || !strcmp("torch", type)) {
			led->num_fnodes++;
		} else if (!strcmp("switch", type)) {
			led->num_snodes++;
		} else {
			pr_err("Invalid type for led node type=%s\n",
					type);
			of_node_put(temp);
			return -EINVAL;
		}
	}

	if (!led->num_fnodes) {
		pr_err("No flash/torch devices defined\n");
		return -ECHILD;
	}

	if (!led->num_snodes) {
		pr_err("No switch devices defined\n");
		return -ECHILD;
	}

	led->fnode = devm_kcalloc(&led->pdev->dev, led->num_fnodes,
				sizeof(*led->fnode), GFP_KERNEL);
	led->snode = devm_kcalloc(&led->pdev->dev, led->num_snodes,
				sizeof(*led->snode), GFP_KERNEL);
	if ((!led->fnode) || (!led->snode))
		return -ENOMEM;

	i = 0;
	for_each_available_child_of_node(node, temp) {
		rc = of_property_read_string(temp, "qcom,type", &type);
		if (rc < 0) {
			pr_err("Failed to parse type, rc=%d\n", rc);
			of_node_put(temp);
			return rc;
		}

		if (!strcmp("flash", type) || !strcmp("torch", type)) {
			rc = register_flash_device(led, &led->fnode[i], temp);
			if (rc < 0) {
				pr_err("Failed to register flash device %s rc=%d\n",
					led->fnode[i].fdev.led_cdev.name, rc);
				of_node_put(temp);
				goto unreg_led;
			}
			led->fnode[i++].fdev.led_cdev.dev->of_node = temp;
		} else {
			rc = register_switch_device(led, &led->snode[j], temp);
			if (rc < 0) {
				pr_err("Failed to register switch device %s rc=%d\n",
					led->snode[j].cdev.name, rc);
				i--;
				of_node_put(temp);
				goto unreg_led;
			}
			led->snode[j++].cdev.dev->of_node = temp;
		}
	}
	led->last_configure = 0;

	return 0;

unreg_led:
	while (i >= 0)
		led_classdev_flash_unregister(&led->fnode[i--].fdev);

	return rc;
}

static bool get_subpmic_handle(struct subpmic_flash_led *led) {
	nu6601_fill_leds_data(led);
	if(NULL != led->ops && NULL != led->private) {
        pr_info("%s:%d get nu6601 leds data success", __func__, __LINE__);
		return true;
	} else {
        pr_err("%s:%d get nu6601 handle failed private:%p, ops:%p\n",
				__func__, __LINE__, led->private, led->ops);
    }

	sc6601_fill_leds_data(led);
	if(NULL != led->ops && NULL != led->private) {
        pr_info("%s:%d get sc6601 leds data success", __func__, __LINE__);
		return true;
	} else {
		pr_err("%s:%d get sc6601 handle failed private:%p, ops:%p\n",
				__func__, __LINE__, led->private, led->ops);
		return false;
	}
}

static int subpmic_flash_led_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct subpmic_flash_led *led;
	int rc;
	static int probe_cnt = 0;

	pr_err("cxc subpmic_flash_led_probe start\n");
	led = devm_kzalloc(&pdev->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	probe_cnt++;
	if(!get_subpmic_handle(led)) {
		if (probe_cnt >= PROBE_CNT_MAX){
			pr_err("cxc: subpmic is not available!! probe_cnt=%d\n", probe_cnt);
			return -EINVAL;
		}
		return -EPROBE_DEFER;
	}

	led->max_channels = (u8)(uintptr_t)of_device_get_match_data(&pdev->dev);
	if (!led->max_channels) {
		pr_err("Failed to get max supported led channels\n");
		return -EINVAL;
	}

	led->pdev = pdev;
	//spin_lock_init(&led->lock);

	rc = subpmic_flash_led_register_device(led, node);
	if (rc < 0) {
		pr_err("Failed to parse and register LED devices rc=%d\n", rc);
		return rc;
	}

	rc = subpmic_flash_led_setup(led);
	if (rc < 0) {
		pr_err("Failed to initialize flash LED, rc=%d\n", rc);
		return rc;
	}
	dev_set_drvdata(&pdev->dev, led);

	pr_err("cxc subpmic_flash_led_probe success and end\n");
	return 0;
}

static int subpmic_flash_led_remove(struct platform_device *pdev)
{
	struct subpmic_flash_led *led = dev_get_drvdata(&pdev->dev);
	int i, j;

	for (i = 0; (i < led->num_snodes); i++) {
		for (j = 0; j < ARRAY_SIZE(subpmic_flash_led_attrs); j++)
			sysfs_remove_file(&led->snode[i].cdev.dev->kobj,
				&subpmic_flash_led_attrs[j].attr);

		led_classdev_unregister(&led->snode[i].cdev);
	}

	for (i = 0; (i < led->num_fnodes); i++)
		led_classdev_flash_unregister(&led->fnode[i].fdev);

	return 0;
}

const static struct of_device_id subpmic_flash_led_match_table[] = {
	{ .compatible = "qcom,subpmic-flash-led", .data = (void *)4, },
	{ },
};

static struct platform_driver subpmic_flash_led_driver = {
	.driver = {
		.name = "leds-subpmic-flash",
		.of_match_table = subpmic_flash_led_match_table,
	},
	.probe = subpmic_flash_led_probe,
	.remove = subpmic_flash_led_remove,
};

module_platform_driver(subpmic_flash_led_driver);

MODULE_DESCRIPTION("subpmic Flash LED driver");
MODULE_LICENSE("GPL v2");





/**
 * Please remove the following macro definition(#define DEBUG) when releasing
 * the official software version.
 */
#define DEBUG
#define pr_fmt(fmt)    TOUCHSCREEN_CLASS_NAME " " DEFAULT_DEVICE_NAME ": " fmt

/**
 * default class name(/sys/class/TOUCHSCREEN_CLASS_NAME).
 * default device name(/sys/class/TOUCHSCREEN_CLASS_NAME/DEFAULT_DEVICE_NAME).
 */
#define TOUCHSCREEN_CLASS_NAME    "touch"
#define DEFAULT_DEVICE_NAME       "tp_dev"

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/msm_drm_notify.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/irqdesc.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include "../../pinctrl/core.h"
#include "../../pinctrl/pinconf.h"
#include <linux/pinctrl/consumer.h>
#include "../../gpio/gpiolib.h"
#include "../../base/base.h"
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include "../../regulator/internal.h"
#include <linux/input/touch-info.h>
#include <linux/pm_wakeirq.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

/**
 * struct tid_private - The private data of touch info device core
 * @mask:		The mask of the gestures enable or disable.
 * @input_dev:		All gesture core will report keycode via this
 *	input device.
 * @wakeup_code:	The key code of the last wakeup system.
 * @wakeup_code_name:	The key code name of the last wakeup
 *	system(i.e. "double_tap" means double-tap).
 * @fb_notifier:	Guess what?
 * @poweron:		Is the screen on?
 * @is_upgrading_firmware:	Is chip upgrading firmware?
 */
struct tid_private {
#ifdef CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT
	atomic_t mask;
	struct input_dev *input_dev;
	atomic_t wakeup_code;
	const char *wakeup_code_name;
#endif
#ifdef CONFIG_FB
	struct notifier_block fb_notifier;
	unsigned int poweron:1;
	unsigned int poweron_early:1;
#endif
	unsigned int regs_enabled:1;
	unsigned int is_upgrading_firmware;
	const char *ini_def_name;
	int dynamic_gpio;
	struct device *dev;
	int reg_count;
	struct regulator **regs;
	struct mutex reg_mutex;
};

/**
 * Do you think 'return dev_get_drvdata(dev)' is better?
 */
static inline struct touch_info_dev *dev_to_tid(struct device *dev)
{
	return container_of(dev, struct touch_info_dev, dev);
}

static inline struct device *tid_to_dev(struct touch_info_dev *tid)
{
	return &tid->dev;
}

#ifdef CONFIG_FB
static int fb_notifier_call(struct notifier_block *nb, unsigned long event,
			    void *data)
{
	int blank;
	int ret = 0;
	struct msm_drm_notifier *evdata = data;
	struct tid_private *p;
	struct touch_info_dev_operations *tid_ops;
	unsigned int mask = 0;
	bool fb_event_blank = false;

	/* If we aren't interested in this event, skip it immediately */
	if (event != MSM_DRM_EVENT_BLANK && event != MSM_DRM_EARLY_EVENT_BLANK)
		return 0;

	if (!evdata || !evdata->data)
		return 0;
	blank = *((int *)evdata->data);
	p = container_of(nb, struct tid_private, fb_notifier);
	if (blank != MSM_DRM_BLANK_POWERDOWN && blank != MSM_DRM_BLANK_UNBLANK) {
		dev_dbg(p->dev, "fb notification: blank = %d\n", blank);
		return 0;
	}

	if (event == MSM_DRM_EVENT_BLANK)
		mask |= BIT(0);
	if (blank == MSM_DRM_BLANK_UNBLANK)
		mask |= BIT(1);

	tid_ops = dev_to_tid(p->dev)->tid_ops;
	switch (mask) {
	case 0x00:
		if (!p->poweron_early)
			break;
		if (tid_ops && tid_ops->suspend_early)
			ret = tid_ops->suspend_early(p->dev->parent);
		p->poweron_early = false;
		break;
	case 0x01:
		fb_event_blank = true;
		if (!p->poweron)
			break;
		if (tid_ops && tid_ops->suspend)
			ret = tid_ops->suspend(p->dev->parent);
		p->poweron = false;
		break;
	case 0x02:
		if (p->poweron_early)
			break;
		if (tid_ops && tid_ops->resume_early)
			ret = tid_ops->resume_early(p->dev->parent);
		p->poweron_early = true;
		break;
	case 0x03:
		fb_event_blank = true;
		if (p->poweron)
			break;
		if (tid_ops && tid_ops->resume)
			ret = tid_ops->resume(p->dev->parent);
		p->poweron = true;
		break;
	}

	dev_info(p->dev, fb_event_blank ? "screen %s\n" : "screen %s %s\n",
		 (fb_event_blank ? p->poweron : p->poweron_early) ?
		 "on" : "off", "early");

	return ret;
}

static inline bool is_poweron(struct device *dev)
{
	return dev_to_tid(dev)->p->poweron;
}

static void fb_notifier_init(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct notifier_block *fb_notifier = &tid->p->fb_notifier;

	tid->p->poweron = true;
	tid->p->poweron_early = true;
	fb_notifier->notifier_call = fb_notifier_call;
	msm_drm_register_client(fb_notifier);
}

static void fb_notifier_remove(struct device *dev)
{
	msm_drm_unregister_client(&dev_to_tid(dev)->p->fb_notifier);
}
#else
static inline void fb_notifier_init(struct device *dev)
{
}

static inline void fb_notifier_remove(struct device *dev)
{
}

static inline bool is_poweron(struct device *dev)
{
	return true;
}
#endif /* CONFIG_FB */

static inline bool is_poweroff(struct device *dev)
{
	return !is_poweron(dev);
}

static ssize_t poweron_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_poweron(dev));
}

static DEVICE_ATTR_RO(poweron);

static ssize_t reset(struct device *dev)
{
	int ret = -ENODEV;
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;

	if (tid_ops && tid_ops->reset)
		ret = tid_ops->reset(dev->parent);

	return ret;
}

static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	return reset(dev) ? : scnprintf(buf, PAGE_SIZE, "reset\n");
}

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int trigger_rst;

	if (kstrtoint(buf, 0, &trigger_rst) || trigger_rst != 1) {
		dev_err(dev, "input parameter is invalid: %s\n", buf);
		return -EINVAL;
	}

	return reset(dev) ? : count;
}

static DEVICE_ATTR_RW(reset);

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", dev_to_tid(dev)->vendor);
}

static DEVICE_ATTR_RO(vendor);

static ssize_t productinfo_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", dev_to_tid(dev)->product);
}

static DEVICE_ATTR_RO(productinfo);

static int get_version(struct device *dev, unsigned int *major,
		       unsigned int *minor)
{
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;
	int ret;
	unsigned int dump;

	if (!tid_ops || !tid_ops->get_version)
		return -ENODEV;

	ret = tid_ops->get_version(dev->parent, major ? : &dump,
				   minor ? : &dump);
	if (ret)
		dev_err(dev, "get version fail\n");

	return ret;
}

static ssize_t buildid_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	unsigned int major, minor;
	int ret;

	ret = get_version(dev, &major, &minor);
	if (ret)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "%04x-%02x\n", major, minor);
}

static DEVICE_ATTR_RO(buildid);

static ssize_t path_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	ssize_t len;
	const char *path;
	struct kobject *kobj = tid->use_dev_path ? &dev->parent->kobj :
			       &dev->kobj;

	path = kobject_get_path(kobj, GFP_KERNEL);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", path ? : "(null)");
	kfree(path);

	return len;
}

static DEVICE_ATTR_RO(path);

static ssize_t flashprog_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", tid->p->is_upgrading_firmware);
}

static DEVICE_ATTR_RO(flashprog);

static int firmware_upgrade(struct device *dev, const char *buf, size_t count,
			    bool force)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	const struct firmware *fw;
	char *c;
	char *name;
	int ret;

	if (is_poweroff(dev)) {
		dev_err(dev, "not allow upgrade firmware(power off)\n");
		return -EPERM;
	}

	if (!tid_ops || !tid_ops->firmware_upgrade)
		return -ENODEV;

	name = kzalloc(count + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	memcpy(name, buf, count);

	c = strnchr(name, count, '\n');
	if (c)
		*c = '\0';

	get_device(dev);
	ret = request_firmware_direct(&fw, name, dev);
	if (ret)
		goto err;

	if (cmpxchg_acquire(&tid->p->is_upgrading_firmware, 0, 1) != 0) {
		dev_info(dev, "is upgrading firmware, please wait\n");
		ret = -EBUSY;
		goto skip_upgrade;
	}

	ret = tid_ops->firmware_upgrade(dev->parent, fw, force);
	smp_store_release(&tid->p->is_upgrading_firmware, 0);

skip_upgrade:
	release_firmware(fw);
err:
	kfree(name);
	put_device(dev);

	return ret ? : count;
}

static size_t get_firmware_name(struct device *dev, char *buf, size_t size);

static ssize_t doreflash_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	return firmware_upgrade(dev, buf, count, false);
}

static ssize_t doreflash_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	char fw_name[64];
	size_t count = get_firmware_name(dev, fw_name, sizeof(fw_name));
	int ret;

	ret = firmware_upgrade(dev, fw_name, count, false);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "doreflash: %s\n", fw_name);
}

static DEVICE_ATTR_RW(doreflash);

static ssize_t forcereflash_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	return firmware_upgrade(dev, buf, count, true);
}

static ssize_t forcereflash_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	char fw_name[64];
	size_t count = get_firmware_name(dev, fw_name, sizeof(fw_name));
	int ret;

	ret = firmware_upgrade(dev, fw_name, count, true);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "forcereflash: %s\n", fw_name);
}

static DEVICE_ATTR_RW(forcereflash);

static inline int gpio_to_pin(struct pinctrl_gpio_range *range,
			      unsigned int gpio)
{
	unsigned int offset = gpio - range->base;

	return range->pins ? range->pins[offset] : range->pin_base + offset;
}

static int pinctrl_get_device_gpio_pin(unsigned int gpio,
				       struct pinctrl_dev **outdev, int *outpin)
{
	struct gpio_desc *gdesc = gpio_to_desc(gpio);
	struct gpio_pin_range *grange;
	struct list_head *head;

	head = &gdesc->gdev->pin_ranges;
	list_for_each_entry(grange, head, node) {
		struct pinctrl_gpio_range *range = &grange->range;

		if (gpio >= range->base && gpio < range->base + range->npins) {
			*outdev = grange->pctldev;
			*outpin = gpio_to_pin(range, gpio);
			return 0;
		}
	}
	return -ENODEV;
}

static int gpio_set_config(unsigned int gpio, unsigned long config)
{
	int ret, pin;
	int selector;
	char pin_group[16];
	const struct pinconf_ops *ops;
	struct pinctrl_dev *pctldev;
	bool retry = false;
	const char *gpio_name = "gpio";

	ret = pinctrl_get_device_gpio_pin(gpio, &pctldev, &pin);
	if (ret)
		return ret;

	mutex_lock(&pctldev->mutex);
	ops = pctldev->desc->confops;
	if (!ops) {
		ret = -ENODEV;
		goto out;
	}

	ret = -ENODEV;
	if (ops->pin_config_set)
		ret = ops->pin_config_set(pctldev, pin, &config, 1);
	if (!ret)
		goto out;

retry:
	snprintf(pin_group, sizeof(pin_group), "%s%d", gpio_name, pin);
	pr_debug("pin group name is '%s'", pin_group);
	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (selector < 0) {
		if (retry) {
			ret = selector;
			goto out;
		}
		retry = true;
		gpio_name = "GPIO";
		goto retry;
	}

	if (ops->pin_config_group_set)
		ret = ops->pin_config_group_set(pctldev, selector, &config, 1);
out:
	mutex_unlock(&pctldev->mutex);

	return ret;
}

static bool gpio_get_config(unsigned int gpio, unsigned long config)
{
	int ret, pin;
	int selector;
	char pin_group[16];
	const struct pinconf_ops *ops;
	struct pinctrl_dev *pctldev;
	bool retry = false;
	const char *gpio_name = "gpio";
	unsigned int param_old = pinconf_to_config_param(config);

	ret = pinctrl_get_device_gpio_pin(gpio, &pctldev, &pin);
	if (ret)
		return false;

	mutex_lock(&pctldev->mutex);
	ops = pctldev->desc->confops;
	if (!ops) {
		ret = -ENODEV;
		goto out;
	}
	ret = -ENODEV;
	if (ops->pin_config_get)
		ret = ops->pin_config_get(pctldev, pin, &config);
	if (ret == -EINVAL)
		goto out;

	if (!ret) {
		/* 1 means the config is set */
		ret = pinconf_to_config_argument(config);
		goto out;
	}

retry:
	snprintf(pin_group, sizeof(pin_group), "%s%d", gpio_name, pin);
	pr_debug("pin group name is '%s'\n", pin_group);
	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (selector < 0) {
		if (retry) {
			ret = selector;
			goto out;
		}
		retry = true;
		gpio_name = "GPIO";
		goto retry;
	}

	if (ops->pin_config_group_get)
		ret = ops->pin_config_group_get(pctldev, selector, &config);
	if (ret)
		goto out;
	/* 1 means the config is set */
	pr_debug("pin group config is 0x%lx(param: 0x%x argument: 0x%x)\n",
		 config, pinconf_to_config_param(config),
		 pinconf_to_config_argument(config));
	if (param_old == pinconf_to_config_param(config))
		ret = pinconf_to_config_argument(config);
out:
	mutex_unlock(&pctldev->mutex);

	return ret > 0 ? true : false;
}

static int touch_gpio_set_config(unsigned int gpio, const char *buf,
				 size_t count)
{
	char *c;
	char arg[16] = { 0 };
	unsigned long config;

	if (!gpio_is_valid(gpio))
		return -EPERM;

	if (count > sizeof(arg) - 1)
		return -EINVAL;
	memcpy(arg, buf, count);
	c = strnchr(arg, count, '\n');
	if (c)
		*c = '\0';
	arg[count] = '\0';

	if (!strcmp(arg, "no_pull")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_DISABLE, 1);
	} else if (!strcmp(arg, "pull_up")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1);
	} else if (!strcmp(arg, "pull_down")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_DOWN, 1);
	} else if (!strcmp(arg, "1") || !strcmp(arg, "high")) {
		gpio_set_value(gpio, 1);
		return 0;
	} else if (!strcmp(arg, "0") || !strcmp(arg, "low")) {
		gpio_set_value(gpio, 0);
		return 0;
	} else {
		return -EINVAL;
	}

	return gpio_set_config(gpio, config);
}

static const char *touch_gpio_get_config(unsigned int gpio)
{
	int i;
	static const char *const pulls[] = {
		"no_pull",
		"pull_up",
		"pull_down",
	};

	for (i = 0; i < ARRAY_SIZE(pulls); i++) {
		int pin_config_param[] = {
			PIN_CONFIG_BIAS_DISABLE,
			PIN_CONFIG_BIAS_PULL_UP,
			PIN_CONFIG_BIAS_PULL_DOWN,
		};
		unsigned long config;

		config = pinconf_to_config_packed(pin_config_param[i], 0);
		if (gpio_get_config(gpio, config))
			return pulls[i];
	}

	return NULL;
}

static struct irq_desc *gpio_to_irq_desc(struct device *dev, unsigned int gpio)
{
	int irq = -ENXIO;

	if (gpio_is_valid(gpio))
		irq = gpio_to_irq(gpio);

	if (irq < 0) {
		irq = of_irq_get(dev->of_node, 0);
		if (irq <= 0)
			return NULL;
	}

	return irq_to_desc(irq);
}

static void gpio_seq_show(struct seq_file *s, struct device *dev,
			  unsigned int gpio)
{
	struct gpio_desc *gdesc = gpio_to_desc(gpio);
	int is_irq = test_bit(FLAG_USED_AS_IRQ, &gdesc->flags);

	gpiod_get_direction(gdesc);
	seq_printf(s, " %3d: gpio-%-3d (%-20.20s) %-13s%-13s%-13s",
		   gpio, gpio_chip_hwgpio(gdesc), gdesc->label,
		   touch_gpio_get_config(gpio) ? : "unknown",
		   test_bit(FLAG_IS_OUT, &gdesc->flags) ? "out" : "in",
		   gpio_get_value(gpio) ? "high" : "low");

	if (is_irq) {
		struct irq_desc *desc = gpio_to_irq_desc(dev, gpio);

		seq_printf(s, "%s", "IRQ");
		if (desc)
			seq_printf(s, "(irq: %3u hwirq: %3lu) %-8s",
				   desc->irq_data.irq, desc->irq_data.hwirq,
				   irqd_is_level_type(&desc->irq_data) ? "Level"
				   : "Edge");
	}
	seq_putc(s, '\n');
}

static ssize_t touch_gpio_show(struct device *dev, unsigned int gpio, char *buf)
{
	size_t count;
	struct seq_file *s;

	if (!gpio_is_valid(gpio))
		return -EPERM;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	s->buf = buf;
	s->size = PAGE_SIZE;
	gpio_seq_show(s, dev, gpio);
	count = s->count;
	kfree(s);

	return count;
}

struct pin_gpio_context {
	unsigned int pin;
	unsigned int gpio;
};

static int pin_range_to_gpio(struct pinctrl_gpio_range *range, unsigned int pin)
{
	if (range->pins) {
		int i;

		for (i = 0; i < range->npins; i++)
			if (range->pins[i] == pin)
				return i + range->base;
	} else {
		unsigned int offset = pin - range->pin_base;

		if (offset >= range->npins)
			return -EINVAL;
		return offset + range->base;
	}

	return -EINVAL;
}

static int gpiochip_match(struct gpio_chip *gc, void *data)
{
	struct pin_gpio_context *pg = data;
	struct gpio_pin_range *grange;
	struct list_head *head;

	head = &gc->gpiodev->pin_ranges;
	list_for_each_entry(grange, head, node) {
		int gpio;
		struct pinctrl_gpio_range *range = &grange->range;

		gpio = pin_range_to_gpio(range, pg->pin);
		if (gpio_is_valid(gpio)) {
			pg->gpio = gpio;
			return true;
		}
	}

	return false;
}

static int pin_to_gpio(unsigned int pin)
{
	struct pin_gpio_context pg;

	pg.pin = pin;
	if (!gpiochip_find(&pg, gpiochip_match))
		return -EINVAL;

	return pg.gpio;
}

static ssize_t export_gpio_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	int gpio;
	unsigned int pin;

	if (kstrtouint(buf, 0, &pin))
		return -EINVAL;

	/* pin number and gpio number may not be equal */
	gpio = pin_to_gpio(pin);
	if (!gpio_is_valid(gpio))
		return gpio;
	tid->p->dynamic_gpio = gpio;
	dev_dbg(dev, "dynamic gpio is: %d", gpio);

	return count;
}

static DEVICE_ATTR_WO(export_gpio);

static ssize_t gpio_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_set_config(tid->p->dynamic_gpio, buf, count) ? :
	       count;
}

static ssize_t gpio_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_show(dev, tid->p->dynamic_gpio, buf);
}

static DEVICE_ATTR_RW(gpio);

static ssize_t irq_gpio_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_set_config(tid->irq_gpio, buf, count) ? : count;
}

static ssize_t irq_gpio_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_show(dev, tid->irq_gpio, buf);
}

static DEVICE_ATTR_RW(irq_gpio);

static ssize_t rst_gpio_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_set_config(tid->rst_gpio, buf, count) ? : count;
}

static ssize_t rst_gpio_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_show(dev, tid->rst_gpio, buf);
}

static DEVICE_ATTR_RW(rst_gpio);

static ssize_t firmware_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	get_firmware_name(dev, buf, PAGE_SIZE);

	return strlcat(buf, "\n", PAGE_SIZE);
}

static DEVICE_ATTR_RO(firmware_name);

static ssize_t disable_depth_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct irq_desc *desc = gpio_to_irq_desc(dev, tid->irq_gpio);

	if (!desc)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", desc->depth);
}

static DEVICE_ATTR_RO(disable_depth);

static ssize_t wake_depth_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct irq_desc *desc = gpio_to_irq_desc(dev, tid->irq_gpio);

	if (!desc)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", desc->wake_depth);
}

static DEVICE_ATTR_RO(wake_depth);

static inline int __regulator_is_enabled(struct regulator_dev *rdev)
{
	/* A GPIO control always takes precedence */
	if (rdev->ena_pin)
		return rdev->ena_gpio_state;

	/* If we don't know then assume that the regulator is always on */
	if (!rdev->desc->ops->is_enabled)
		return 1;

	return rdev->desc->ops->is_enabled(rdev);
}

static void regulator_consumer_show(struct seq_file *s,
				    struct regulator_dev *rdev)
{
	struct regulator *reg;

	mutex_lock(&rdev->mutex);
	/* Print a header if there are consumers. */
	if (rdev->open_count)
		seq_printf(s,
			   "%-64s %-10s Min_uV   Max_uV  load_uA   %-16s use_count: %-4u enabled: %-3c\n",
			   "Device-Supply", "EN", rdev->desc->name,
			   rdev->use_count,
			   __regulator_is_enabled(rdev) ? 'Y' : 'N');

	list_for_each_entry(reg, &rdev->consumer_list, list)
		seq_printf(s, "%-64s %c(%3d)   %8d %8d %8d\n",
			   reg->supply_name ? : "(null)",
			   reg->enabled ? 'Y' : 'N', reg->enabled,
			   reg->min_uV, reg->max_uV, reg->uA_load);

	mutex_unlock(&rdev->mutex);
}

static void regulator_consumers_show(struct seq_file *s, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct property *prop;

	if (!np)
		return;

	for_each_property_of_node(np, prop) {
		char *find;
		char *name;
		struct regulator *reg;
		struct regulator_dev *rdev;

		find = strnstr(prop->name, "-supply", strlen(prop->name));
		if (!find || strcmp(find, "-supply"))
			continue;
		name = kzalloc(find - prop->name + 1, GFP_KERNEL);
		if (!name)
			return;
		memcpy(name, prop->name, find - prop->name);
		dev_dbg(dev, "regulator name is '%s'\n", prop->name);
		reg = regulator_get(dev, name);
		kfree(name);
		if (IS_ERR(reg)) {
			dev_err(dev, "get regulator(%s) fail\n", prop->name);
			continue;
		}
		rdev = reg->rdev;
		regulator_put(reg);
		regulator_consumer_show(s, rdev);
		seq_putc(s, '\n');
	}
}

static ssize_t regulator_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	size_t count;
	struct seq_file *s;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	s->buf = buf;
	s->size = PAGE_SIZE;
	regulator_consumers_show(s, dev);
	count = s->count;
	kfree(s);

	return count;
}

static DEVICE_ATTR_RO(regulator);

static ssize_t hardware_info_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	tid_hardware_info_get(buf, PAGE_SIZE);

	return strlcat(buf, "\n", PAGE_SIZE);
}

static DEVICE_ATTR_RO(hardware_info);

static ssize_t enable_regulators_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	bool enable = dev_to_tid(dev)->p->regs_enabled;

	return scnprintf(buf, PAGE_SIZE, "%s\n", enable ? "enable" : "disable");
}

static ssize_t enable_regulators_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int enable;
	int ret;

	if (kstrtoint(buf, 0, &enable)) {
		dev_err(dev, "input parameter is invalid: %s\n", buf);
		return -EINVAL;
	}

	ret = tid_regulators_enable_opt(dev_to_tid(dev), !!enable);

	return ret ? : count;
}

static DEVICE_ATTR_RW(enable_regulators);

static ssize_t fod_status_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = 0;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	int fod_status;

	if (kstrtouint(buf, 0, &fod_status))
		return -EINVAL;

	if (fod_status == tid->fod_status) {
		dev_info(dev, "fod_status set interface is repetitive\n");
		return count;
	} else {
		tid->fod_status = fod_status;
	}

	if (!tid_ops || !tid_ops->fod_status_set) {
		dev_info(dev, "fod_status set interface is invalid\n");
		return -ENODEV;
	}

	ret = tid_ops->fod_status_set(dev->parent, tid->fod_status);
	if (ret < 0) {
		dev_info(dev, "fod_status set %d fail\n",
			 tid->fod_status);
		tid->fod_status = -1;
	}

	return count;

}

static ssize_t fod_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", tid->fod_status);
}

static DEVICE_ATTR_RW(fod_status);

static ssize_t aod_status_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (kstrtouint(buf, 0, &tid->aod_status)) {
		dev_info(dev, "aod_status set interface is invalid\n");
		tid->aod_status = -1;
		return -EINVAL;
	}

	dev_info(dev, "aod_status set %d success\n", tid->aod_status);
	return count;

}

static ssize_t aod_status_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", tid->aod_status);
}

static DEVICE_ATTR_RW(aod_status);

static ssize_t grip_area_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int ret = 0;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;

	if (kstrtouint(buf, 0, &tid->grip_area))
		return -EINVAL;

	if (!tid_ops || !tid_ops->grip_area_set) {
		dev_info(dev, "grip_area set interface is invalid\n");
		return -ENODEV;
	}

	ret = tid_ops->grip_area_set(dev->parent, tid->grip_area);
	if (ret < 0) {
		dev_info(dev, "grip_area set %d fail\n",
			 tid->grip_area);
		tid->grip_area = -1;
	}

	return count;
}

static ssize_t grip_area_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	return scnprintf(buf, PAGE_SIZE, "%d\n", tid->grip_area);
}

static DEVICE_ATTR_RW(grip_area);

#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST
static ssize_t ini_file_name_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	char *c;
	char *name;
	struct touch_info_dev *tid = dev_to_tid(dev);

	name = devm_kmalloc(dev, count + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	if (tid->p->ini_def_name)
		devm_kfree(dev, (void *)tid->p->ini_def_name);
	tid->p->ini_def_name = name;
	memcpy(name, buf, count);

	c = strnchr(name, count, '\n');
	if (c)
		*c = '\0';
	name[count] = 0;

	if (!strcmp(name, "off")) {
		devm_kfree(dev, name);
		tid->p->ini_def_name = NULL;
		name = "default setting";
	}
	dev_dbg(dev, "modify ini file name to '%s'\n", name);

	return count;
}

static size_t get_ini_name(struct device *dev, char *buf, size_t size);

static ssize_t ini_file_name_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (tid->p->ini_def_name)
		return scnprintf(buf, PAGE_SIZE, "%s\n", tid->p->ini_def_name);

	get_ini_name(dev, buf, PAGE_SIZE);

	return strlcat(buf, "\n", PAGE_SIZE);
}

static DEVICE_ATTR_RW(ini_file_name);

static int open_short_test(struct device *dev, bool force);

static ssize_t open_short_test_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	char *c;
	char *name;
	const char *orig_name;
	struct touch_info_dev *tid = dev_to_tid(dev);
	int ret;

	name = kmalloc(count + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	memcpy(name, buf, count);

	c = strnchr(name, count, '\n');
	if (c)
		*c = '\0';
	name[count] = 0;

	orig_name = tid->p->ini_def_name;
	tid->p->ini_def_name = name;

	ret = open_short_test(dev, false);

	tid->p->ini_def_name = orig_name;
	kfree(name);

	if (ret < 0)
		return ret;
	dev_info(dev, "open-short test %s\n", ret ? "success" : "fail");

	return count;
}

static ssize_t open_short_test_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int ret = open_short_test(dev, false);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "result=%d\n", !!ret);
}

static DEVICE_ATTR_RW(open_short_test);

#define INI_PULL_PATH_PREFIX "/data/vendor/fac_sources/"

static ssize_t pull_ini(struct device *dev, const char *path)
{
	char name[64];
	struct file *file;
	loff_t pos = 0;
	int ret;
	const struct firmware *fw;

	get_ini_name(dev, name, PAGE_SIZE);
	ret = request_firmware_direct(&fw, name, dev);
	if (ret)
		return ret;

	file = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(file)) {
		ret = PTR_ERR(file);
		goto release_fw;
	}

	ret = kernel_write(file, fw->data, fw->size, &pos);
	filp_close(file, NULL);

release_fw:
	release_firmware(fw);

	return ret;
}

static ssize_t pull_ini_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	char *c;
	char *path;
	int ret;

	path = kmalloc(count + 1, GFP_KERNEL);
	if (!path)
		return -ENOMEM;
	memcpy(path, buf, count);
	c = strnchr(path, count, '\n');
	if (c)
		*c = '\0';
	path[count] = 0;

	ret = pull_ini(dev, path);
	kfree(path);

	return ret < 0 ? ret : count;
}

static ssize_t pull_ini_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	char name[64];
	char path[128];
	int ret;

	get_ini_name(dev, name, PAGE_SIZE);
	snprintf(path, sizeof(path), "%s%s", INI_PULL_PATH_PREFIX, name);

	ret = pull_ini(dev, path);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "pull ini path: %s\n", path);
}

static DEVICE_ATTR_RW(pull_ini);
#endif /* CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST */

#ifdef CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT
static ssize_t gesture_data_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			 atomic_read(&tid->p->wakeup_code));
}

static DEVICE_ATTR_RO(gesture_data);

static ssize_t gesture_name_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", tid->p->wakeup_code_name);
}

static DEVICE_ATTR_RO(gesture_name);

static ssize_t gesture_control_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%x\n", atomic_read(&tid->p->mask));
}

static int gesture_set_capability(struct device *dev, unsigned int mask)
{
	bool enable;
	bool enabled;
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (is_poweroff(dev)) {
		dev_err(dev, "not allow gesture control(power off)\n");
		return -EPERM;
	}

	enable = !!(mask & BIT(GS_KEY_ENABLE));
	enabled = !!(atomic_read(&tid->p->mask) & BIT(GS_KEY_ENABLE));
	if (enable) {
		mask &= BIT(GS_KEY_ENABLE) | (BIT(GS_KEY_END) - 1);
		dev_dbg(dev, "enable gesture, mask: 0x%08x\n", mask);
	} else {
		mask = 0;
		dev_dbg(dev, "disable all gesture\n");
	}
	atomic_set(&tid->p->mask, mask);

	if (enable != enabled) {
		int irq = -EINVAL;
		struct touch_info_dev_operations *tid_ops = tid->tid_ops;

		if (tid_ops && tid_ops->gesture_set_capability) {
			int ret = tid_ops->gesture_set_capability(dev->parent,
								  enable);
			if (ret)
				return ret;
		}
		if (gpio_is_valid(tid->irq_gpio))
			irq = gpio_to_irq(tid->irq_gpio);
		if (irq < 0 && dev->of_node)
			irq = of_irq_get(dev->of_node, 0);
		if (irq < 0) {
			dev_err(dev, "%s: irq is invalid with errno: %d\n",
				__func__, irq);
			return irq;
		}

		if (enable)
			dev_pm_set_wake_irq(dev, irq);
		else
			dev_pm_clear_wake_irq(dev);
	}

	return 0;
}

static ssize_t gesture_control_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	unsigned int mask;

	if (count != sizeof(mask)) {
		if (kstrtouint(buf, 16, &mask)) {
			dev_dbg(dev, "input parameter is invalid: %s\n", buf);
			return -EINVAL;
		}
	} else {
		memcpy(&mask, buf, count);
	}

	ret = gesture_set_capability(dev, mask);

	return ret ? : count;
}

static DEVICE_ATTR_RW(gesture_control);
#endif /* CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT */

static struct attribute *touch_info_dev_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_buildid.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_poweron.attr,
	&dev_attr_path.attr,
	&dev_attr_vendor.attr,
	&dev_attr_export_gpio.attr,
	&dev_attr_gpio.attr,
	&dev_attr_irq_gpio.attr,
	&dev_attr_rst_gpio.attr,
	&dev_attr_firmware_name.attr,
	&dev_attr_disable_depth.attr,
	&dev_attr_wake_depth.attr,
	&dev_attr_regulator.attr,
	&dev_attr_hardware_info.attr,
	&dev_attr_enable_regulators.attr,
	&dev_attr_fod_status.attr,
	&dev_attr_aod_status.attr,
	&dev_attr_grip_area.attr,
#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST
	&dev_attr_ini_file_name.attr,
	&dev_attr_open_short_test.attr,
	&dev_attr_pull_ini.attr,
#endif
#ifdef CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT
	&dev_attr_gesture_data.attr,
	&dev_attr_gesture_name.attr,
	&dev_attr_gesture_control.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(touch_info_dev);

static struct class touchscreen_class = {
	.owner		= THIS_MODULE,
	.name		= TOUCHSCREEN_CLASS_NAME,
	.dev_groups	= touch_info_dev_groups,
};

static int __match_name(struct device *dev, const void *data)
{
	return !strcmp(dev_name(dev), data);
}

static struct touch_info_dev *find_tid_by_name(const char *name)
{
	struct device *dev;

	dev = class_find_device(&touchscreen_class, NULL, name, __match_name);
	if (!dev)
		return NULL;
	put_device(dev);

	return dev_to_tid(dev);
}

static struct touch_info_dev *find_default_tid(void)
{
	struct touch_info_dev *tid;

	tid = find_tid_by_name(DEFAULT_DEVICE_NAME);
	if (!tid) {
		pr_info("any devices is not found\n");
		return NULL;
	}

	return tid;
}

#ifdef CONFIG_TOUCHSCREEN_TID_LOCKDOWNINFO_SUPPORT
#define LOCKDOWN_INFO_MAGIC_BASE    0x31

static int get_lockdown_info(struct device *dev, char *buf)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	static char lockdown_buf[LOCKDOWN_INFO_SIZE];
	static bool lockdown_valid;

retry:
	if (likely(lockdown_valid)) {
		memcpy(buf, lockdown_buf, sizeof(lockdown_buf));
		pr_info("get lockdowninfo repeatability\n");
		return 0;
	}

	if (!tid_ops || !tid_ops->get_lockdown_info)
		return -ENODEV;

	ret = tid_ops->get_lockdown_info(dev->parent, lockdown_buf);
	if (!ret) {
		lockdown_valid = true;
		goto retry;
	}

	return ret;
}

static const char *const panel_makers[] = {
	"biel-tpb",	/* 0x31 */
	"lens",
	"wintek",
	"ofilm",
	"biel-d1",	/* 0x35 */
	"tpk",
	"laibao",
	"sharp",
	"jdi",
	"eely",		/* 0x40 */
	"gis-ebbg",
	"lgd",
	"auo",
	"boe",
	"ds-mudong",	/* 0x45 */
	"tianma",
	"truly",
	"samsung",
	"primax",
	"cdot",		/* 0x50 */
	"visionox",
	"txd",
	"hlt",
};

static const char *get_panel_maker(struct device *dev)
{
	int index;
	char buf[LOCKDOWN_INFO_SIZE];
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (likely(tid->panel_maker))
		return tid->panel_maker;

	if (get_lockdown_info(dev, buf)) {
		dev_err(dev, "get panel maker fail\n");
		return NULL;
	}

	/**
	 * why is 6? 0x*a, 0x*b, 0x*c, 0x*d, 0x*e, and 0x*f is ignored.
	 */
	index = buf[LOCKDOWN_INFO_PANEL_MAKER_INDEX] - LOCKDOWN_INFO_MAGIC_BASE;
	index -= ((index + 1) >> 4) * 6;
	if (index >= ARRAY_SIZE(panel_makers)) {
		dev_err(dev, "panel maker lockdown info is invalid\n");
		return NULL;
	}
	tid->panel_maker = panel_makers[index];

	return panel_makers[index];
}

static const char *const panel_colors[] = {
	"white",	/* 0x31 */
	"black",
	"red",
	"yellow",
	"green",	/* 0x35 */
	"pink",
	"purple",
	"golden",
	"silver",
	"gray",		/* 0x40 */
	"blue",
	"pink-purple",
};

static const char *get_panel_color(struct device *dev)
{
	int index;
	char buf[LOCKDOWN_INFO_SIZE];
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (likely(tid->panel_color))
		return tid->panel_color;

	if (get_lockdown_info(dev, buf)) {
		dev_err(dev, "get panel color fail\n");
		return NULL;
	}

	/**
	 * why is 6? 0x*a, 0x*b, 0x*c, 0x*d, 0x*e, and 0x*f is ignored.
	 */
	index = buf[LOCKDOWN_INFO_PANEL_COLOR_INDEX] - LOCKDOWN_INFO_MAGIC_BASE;
	index -= ((index + 1) >> 4) * 6;
	if (index >= ARRAY_SIZE(panel_colors)) {
		dev_err(dev, "panel color lockdown info is invalid\n");
		return NULL;
	}
	tid->panel_color = panel_colors[index];

	return panel_colors[index];
}

static int get_hardware_id(struct device *dev)
{
	char buf[LOCKDOWN_INFO_SIZE];

	if (get_lockdown_info(dev, buf)) {
		dev_err(dev, "get hardware id fail\n");
		return 0;
	}

	return buf[LOCKDOWN_INFO_HW_VERSION_INDEX];
}
#else
static inline const char *get_panel_maker(struct device *dev)
{
	return dev_to_tid(dev)->panel_maker;
}

static inline const char *get_panel_color(struct device *dev)
{
	return dev_to_tid(dev)->panel_color;
}

static inline int get_hardware_id(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_TOUCHSCREEN_TID_LOCKDOWNINFO_SUPPORT */

#ifdef CONFIG_TOUCHSCREEN_TID_PROC_SUPPORT
#define TOUCH_PROC_DIR      "touchscreen"
#define GESTURE_PROC_DIR    "gesture"

/**
 * open-short test implementation
 */
#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST
#define TOUCH_OS_TEST    "ctp_openshort_test"

#ifdef CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST_STORE_RESULT
#define RESULT_PATH "/data/vendor/fac_sources/open_short_result.txt"

static int open_short_write_to_file(const void *buf, size_t count)
{
	int ret;
	struct file *file;
	loff_t pos = 0;
	struct inode *inode;

	if (!buf || !count)
		return -EINVAL;

	file = filp_open(RESULT_PATH, O_RDWR | O_APPEND | O_CREAT, 0600);
	if (IS_ERR(file))
		return PTR_ERR(file);

	inode = file->f_inode;
	/**
	 * If the file size exceeds 2MB, we delete the file.
	 */
	if (unlikely(i_size_read(inode) > SZ_2M)) {
		pr_debug("file size exceeds 2MB\n");
		filp_close(file, NULL);
		file = filp_open(RESULT_PATH, O_RDWR | O_TRUNC | O_CREAT, 0600);
		if (IS_ERR(file))
			return PTR_ERR(file);
	}

	ret = kernel_write(file, buf, count, &pos);
	if (ret < 0)
		pr_debug("error writing open-short result file: %s\n",
			 RESULT_PATH);
	filp_close(file, NULL);

	return (ret < 0) ? -EIO : 0;
}

static int seq_file_buf_init(struct seq_file *s, int pages)
{
	s->count = 0;
	s->size = PAGE_SIZE * pages;
	s->buf = vmalloc(s->size);
	if (!s->buf) {
		s->size = 0;
		return -ENOMEM;
	}

	return 0;
}
#else
static int open_short_write_to_file(const void *buf, size_t count)
{
	return 0;
}

static int seq_file_buf_init(struct seq_file *s, int pages)
{
	s->buf = NULL;
	s->size = 0;
	s->count = 0;

	return 0;
}
#endif /* CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST_STORE_RESULT */

static const char *get_panel_maker(struct device *dev);
static const char *get_panel_color(struct device *dev);

static size_t get_ini_name(struct device *dev, char *buf, size_t size)
{
	const char *color = NULL;
	struct touch_info_dev *tid = dev_to_tid(dev);

	get_panel_maker(dev);
	if (tid->ini_name_use_color)
		color = get_panel_color(dev);

	return snprintf(buf, size, color ? "%s-%s-%s-%s.ini" : "%s-%s-%s.ini",
			tid->vendor, tid->product, tid->panel_maker ? : "none",
			color);
}

static int open_short_test(struct device *dev, bool force)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	struct seq_file *s;
	char ini_name[64];
	const struct firmware *fw = NULL;
	const char *ini_def_name = tid->p->ini_def_name;
	int retry = 0;
	static int pages = 4;
	bool pages_adjust = false;

	if (is_poweroff(dev) && !force) {
		dev_err(dev, "not allow open-short test(power off)\n");
	}

	if (force)
		dev_info(dev,
			"force open-short test(please be careful if power off) with screen %s\n",
			is_poweron(dev) ? "on" : "off");

	if (!tid_ops || !tid_ops->open_short_test) {
		dev_info(dev, "open-short test interface is invalid\n");
		return -ENODEV;
	}

	if (!ini_def_name) {
		get_ini_name(dev, ini_name, ARRAY_SIZE(ini_name));
		ini_def_name = ini_name;
	}

	get_device(dev);
	if (!tid->open_short_not_use_fw) {
		dev_dbg(dev, "ini file name is '%s'\n", ini_def_name);
		ret = request_firmware_direct(&fw, ini_def_name, dev);
		if (ret)
			goto put_dev;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s) {
		ret = -ENOMEM;
		goto release_fw;
	}
	ret = seq_file_buf_init(s, pages);
	if (ret) {
		dev_err(dev,
			"%s: alloc memory fail, result will not write to file\n",
			__func__);
		/**
		 * Should we stop? If we continue, the result of this
		 * open-short test will not write to the file. But I don't
		 * care. Do you care?
		 *
		 * kfree(s);
		 * ret = -ENOMEM;
		 * goto release_fw;
		 */
	}

	dev_info(dev, "open-short test start\n");
retry:
	ret = tid_ops->open_short_test(dev->parent, s, fw);
	if (ret < 0) {
		dev_info(dev, "%s fail with errno: %d\n", "open-short test",
			 ret);
		seq_printf(s, "%s fail with errno: %d\n", "open-short test",
			   ret);
	} else {
		dev_info(dev, "%s %s\n", "open-short test",
			 ret ? "pass" : "fail");
		seq_printf(s, "%s %s\n", "open-short test",
			   ret ? "pass" : "fail");
	}
	seq_putc(s, '\n');

	if (ret <= 0 && retry++ < 3) {
		dev_info(dev, "open-short test fail and retry %d times\n",
			 retry);
		msleep(20);
		s->count = 0;
		goto retry;
	}

	if (retry)
		seq_printf(s, "open-short test retry %d time\n", retry);

	if (s->count && open_short_write_to_file(s->buf, s->count))
		dev_err(dev, "write open short test result file fail\n");

	if (ret > 0 && s->count) {
		if (unlikely(s->count == s->size)) {
			pages <<= 1;
			pages_adjust = true;
		} else if (unlikely((s->size - s->count) / PAGE_SIZE > 1)) {
			pages -= (s->size - s->count) / PAGE_SIZE - 1;
			if (pages < 1)
				pages = 1;
			pages_adjust = true;
		}
	}

	if (unlikely(pages_adjust))
		dev_info(dev, "automatically adjust the buffer size to %d KB\n",
			 pages << (PAGE_SHIFT - 10));

	vfree(s->buf);
	kfree(s);
release_fw:
	release_firmware(fw);
put_dev:
	put_device(dev);

	return ret;
}

static int open_short_proc_show(struct seq_file *seq, void *offset)
{
	int ret = open_short_test(seq->private, false);

	if (ret < 0)
		return ret;
	seq_printf(seq, "result=%d\n", !!ret);

	return 0;
}

PROC_ENTRY_RO(open_short);

static inline void open_short_proc_create(struct device *dev)
{
	proc_create_data(TOUCH_PROC_DIR "/" TOUCH_OS_TEST, 0444, NULL,
			 &open_short_fops, dev);
}

static inline void open_short_proc_remove(void)
{
	remove_proc_entry(TOUCH_PROC_DIR "/" TOUCH_OS_TEST, NULL);
}
#else
static inline void open_short_proc_create(struct device *dev)
{
}

static inline void open_short_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_TID_OPENSHORT_TEST */

/**
 * lockdown information show implementation
 */
#ifdef CONFIG_TOUCHSCREEN_TID_LOCKDOWNINFO_SUPPORT
#define TOUCH_LOCKDOWN_INFO	"lockdown_info"
#define INSCREEN_FP_MODE	"inscreen_fp_mode"

static int lockdown_info_proc_show(struct seq_file *seq, void *offset)
{
	int i;
	int ret;
	char buf[LOCKDOWN_INFO_SIZE];

	ret = get_lockdown_info(seq->private, buf);
	if (ret)
		return ret;

	/* lockdown info is only LOCKDOWN_INFO_SIZE bytes */
	for (i = 0; i < sizeof(buf); i++)
		seq_printf(seq, "%02x", buf[i]);
	seq_putc(seq, '\n');

	return ret;
}

PROC_ENTRY_RO(lockdown_info);

static int inscreen_fp_mode_proc_show(struct seq_file *seq, void *offset)
{

	return 0;
}
PROC_ENTRY_RO(inscreen_fp_mode);

static inline void lockdown_info_proc_create(struct device *dev)
{
	proc_create_data(TOUCH_PROC_DIR "/" TOUCH_LOCKDOWN_INFO, 0444, NULL,
			 &lockdown_info_fops, dev);
}

static inline void inscreen_fingerprit_mode_create(struct device *dev)
{
	proc_create_data(TOUCH_PROC_DIR "/" INSCREEN_FP_MODE, 0444, NULL,
			 &lockdown_info_fops, dev);
}

static inline void inscreen_fingerprit_mode_remove(void)
{
	remove_proc_entry(TOUCH_PROC_DIR "/" INSCREEN_FP_MODE, NULL);
}

static inline void lockdown_info_proc_remove(void)
{
	remove_proc_entry(TOUCH_PROC_DIR "/" TOUCH_LOCKDOWN_INFO, NULL);
}
#else
static inline void lockdown_info_proc_create(struct device *dev)
{
}

static inline void lockdown_info_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_TID_LOCKDOWNINFO_SUPPORT */

/**
 * gesture on/off and data implementation
 */
#ifdef CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT
#define GESTURE_ON_OFF      "onoff"
#define GESTURE_DATA        "data"

/**
 * gesture on/off implementation
 */
static int gesture_control_proc_show(struct seq_file *seq, void *offset)
{
	struct touch_info_dev *tid = dev_to_tid(seq->private);
	unsigned int bit = BIT(GS_KEY_ENABLE) | BIT(GS_KEY_DOUBLE_TAP);

	seq_printf(seq, "%d\n", (atomic_read(&tid->p->mask) & bit) == bit);

	return 0;
}

static ssize_t gesture_control_proc_write(struct file *file,
					  const char __user *ubuf, size_t size,
					  loff_t *ppos)
{
	int ret;
	struct seq_file *seq = file->private_data;
	struct device *dev = seq->private;
	char buf[4];
	unsigned int mask;

	if (size > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '1' || !strncmp(buf, "on", 2))
		mask = BIT(GS_KEY_ENABLE) | BIT(GS_KEY_DOUBLE_TAP);
	else if (buf[0] == '0' || !strncmp(buf, "off", 3))
		mask = 0;
	else
		return -EINVAL;

	ret = gesture_set_capability(dev, mask);
	*ppos += size;

	return ret ? : size;
}

PROC_ENTRY_RW(gesture_control);

/**
 * gesture data implementation
 */
static int gesture_data_proc_show(struct seq_file *seq, void *offset)
{
	seq_puts(seq, "K\n");

	return 0;
}

PROC_ENTRY_RO(gesture_data);

static inline void gesture_proc_create(struct device *dev)
{
	proc_create_data(GESTURE_PROC_DIR "/" GESTURE_ON_OFF, 0666, NULL,
			 &gesture_control_fops, dev);
	proc_create_data(GESTURE_PROC_DIR "/" GESTURE_DATA, 0444, NULL,
			 &gesture_data_fops, dev);
}

static inline void gesture_proc_remove(void)
{
	remove_proc_entry(GESTURE_PROC_DIR "/" GESTURE_ON_OFF, NULL);
	remove_proc_entry(GESTURE_PROC_DIR "/" GESTURE_DATA, NULL);
}
#else
static inline void gesture_proc_create(struct device *dev)
{
}

static inline void gesture_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT */

static int grip_area_debugfs_show(struct seq_file *seq, void *offset)
{
	struct touch_info_dev *tid = seq->private;

	seq_printf(seq, "grip_area=%d\n", tid->grip_area);
	return 0;

}

static ssize_t grip_area_debugfs_write(struct file *file,
					  const char __user *ubuf, size_t size,
					  loff_t *ppos)
{
	int ret = 0;
	char buf[16];
	struct seq_file *seq = file->private_data;
	struct touch_info_dev *tid = seq->private;
	struct device *dev = &tid->dev;
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;

	if (!tid_ops || !tid_ops->grip_area_set) {
		dev_info(dev, "grip_area set interface is invalid\n");
		return -ENODEV;
	}

	if (size >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, size))
		return -EFAULT;

	buf[size] = '\0';
	if (kstrtoint(buf, 0, &tid->grip_area))
		return -EINVAL;

	ret = tid_ops->grip_area_set(dev->parent, tid->grip_area);
	if (ret < 0) {
		dev_info(dev, "grip_area set %d fail\n",
			 tid->grip_area);
		tid->grip_area = -1;
	}

	return ret < 0 ? ret : size;
}

DEBUGFS_ENTRY_RW(grip_area);

/**
 * The first device of create proc node. Beacause the same proc node should be
 * created only once.
 */
static inline struct device **get_proc_owner(void)
{
	static struct device *owner;

	return &owner;
}

static void touch_proc_add_device(struct device *dev)
{
	struct device **owner = get_proc_owner();

	BUG_ON(!dev);
	if (*owner) {
		dev_info(dev, "create proc again, just return\n");
		return;
	}
	*owner = dev;

	open_short_proc_create(dev);
	lockdown_info_proc_create(dev);
	gesture_proc_create(dev);
}

static void touch_proc_del_device(struct device *dev)
{
	struct device **owner = get_proc_owner();

	if (*owner != dev)
		return;
	*owner = NULL;

	open_short_proc_remove();
	lockdown_info_proc_remove();
	gesture_proc_remove();
}

static void create_proc(void)
{
	proc_mkdir(TOUCH_PROC_DIR, NULL);
	proc_mkdir(GESTURE_PROC_DIR, NULL);
}
#else
static inline void touch_proc_add_device(struct device *dev)
{
}

static inline void touch_proc_del_device(struct device *dev)
{
}

static inline void create_proc(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_TID_PROC_SUPPORT */

#ifdef CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT
enum {
	/* tap */
	DOUBLE_TAP	= 0x270,
	ONECE_TAP,
	LONG_PRESS,

	/* swipe */
	SWIPE_X_LEFT	= 0x280,
	SWIPE_X_RIGHT,
	SWIPE_Y_UP,
	SWIPE_Y_DOWN,

	/* unicode */
	UNICODE_E	= 0x290,
	UNICODE_C,
	UNICODE_W,
	UNICODE_M,
	UNICODE_O,
	UNICODE_S,
	UNICODE_V,
	UNICODE_Z	= UNICODE_V + 4,
};

/**
 * @support_codes, @code_names and @enum gesture_key must match one by one.
 *
 * Note: @enum gesture_key defined in touch-info.h.
 */
static const unsigned int support_codes[] = {
	SWIPE_X_LEFT,
	SWIPE_X_RIGHT,
	SWIPE_Y_UP,
	SWIPE_Y_DOWN,
	DOUBLE_TAP,
	ONECE_TAP,
	LONG_PRESS,
	UNICODE_E,
	UNICODE_C,
	UNICODE_W,
	UNICODE_M,
	UNICODE_O,
	UNICODE_S,
	UNICODE_V,
	UNICODE_Z,
};

static const char *const code_names[] = {
	"swipe_left",
	"swipe_right",
	"swipe_up",
	"swipe_down",
	"double_tap",
	"once_tap",
	"long_press",
	"e",
	"c",
	"w",
	"m",
	"o",
	"s",
	"v",
	"z",
};

static int gesture_report_input_dev_init(struct device *dev)
{
	int i;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct input_dev *input_dev = tid->p->input_dev;
	unsigned int codes[ARRAY_SIZE(support_codes)];

	/* Init and register input device */
	input_dev->name = "touchpanel-input";
	input_dev->id.bustype = BUS_HOST;
	input_set_drvdata(input_dev, tid);

	if (!of_property_read_u32_array(dev->of_node, "touchpanel,codes", codes,
					ARRAY_SIZE(codes))) {
		unsigned int *code = (void *)support_codes;

		for (i = 0; i < ARRAY_SIZE(codes); i++, code++) {
			if (!codes[i])
				continue;
			dev_dbg(dev,
				"modify code(support_codes[%d]) from 0x%x to 0x%x\n",
				i, support_codes[i], codes[i]);
			/**
			 * Do you think I am crazy?
			 * I am trying to modify a read-only variable.
			 */
			*code = codes[i];
		}
	}
	for (i = 0; i < ARRAY_SIZE(support_codes); i++)
		input_set_capability(input_dev, EV_KEY, support_codes[i]);

	return input_register_device(input_dev);
}

static int gesture_report_init(struct device *dev)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct tid_private *p = tid->p;

	BUILD_BUG_ON(ARRAY_SIZE(support_codes) != GS_KEY_END);
	BUILD_BUG_ON(ARRAY_SIZE(code_names) != GS_KEY_END);

	/**
	 * Allocate and register input device.
	 */
	p->input_dev = devm_input_allocate_device(dev);
	if (!p->input_dev) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}
	ret = gesture_report_input_dev_init(dev);
	if (ret) {
		dev_err(dev, "failed to register input device\n");
		return ret;
	}
	device_init_wakeup(dev, true);

	return 0;
}

/**
 * tid_report_key: - report new input event
 * @tid: touch info device
 * @key: event code
 */
int tid_report_key(struct touch_info_dev *tid, enum gesture_key key)
{
	struct device *dev;
	unsigned int code;
	unsigned int mask;
	const char *gesture_name;

	if (WARN_ON(key >= GS_KEY_END))
		return -EINVAL;

	if (!tid)
		return -ENODEV;

	dev = tid_to_dev(tid);
	if (unlikely(!device_is_registered(dev))) {
		pr_err("device is not registered\n");
		return -ENODEV;
	}

	get_device(dev);
	code = support_codes[key];
	gesture_name = code_names[key];
	mask = atomic_read(&tid->p->mask);
	if (!(mask & BIT(GS_KEY_ENABLE))) {
		dev_dbg(dev,
			"all gestures has disabled, ignore this code: 0x%x(%s)\n",
			code, gesture_name);
		goto out;
	}

	if (mask & BIT(key)) {
		struct input_dev *input_dev = tid->p->input_dev;

		tid->p->wakeup_code_name = gesture_name;
		atomic_set(&tid->p->wakeup_code, code);
		input_report_key(input_dev, code, 1);
		input_sync(input_dev);
		input_report_key(input_dev, code, 0);
		input_sync(input_dev);

		dev_dbg(dev, "input report keycode: 0x%x(%s)\n", code,
			gesture_name);
	} else {
		dev_dbg(dev, "ignore code: 0x%x(%s), according to mask: %08x\n",
			code, gesture_name, mask);
	}
out:
	put_device(dev);

	return 0;
}
EXPORT_SYMBOL(tid_report_key);

/**
 * Return %true if gesture is enabled, otherwise return %false.
 */
bool tid_gesture_is_enabled(struct touch_info_dev *tid)
{
	if (!tid)
		tid = find_default_tid();

	return tid ? !!(atomic_read(&tid->p->mask) & BIT(GS_KEY_ENABLE)) :
	       false;
}
EXPORT_SYMBOL(tid_gesture_is_enabled);
#else
static int gesture_report_init(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_TOUCHSCREEN_TID_GESTURE_SUPPORT */

void create_debug(void *data)
{
	struct dentry *debugfs_root;

	debugfs_root = debugfs_create_dir("tp_debug", NULL);
	if (!debugfs_root)
		pr_warn("tp_debug: Failed to create debugfs directory\n");

	debugfs_create_file("grip_area", 0666, debugfs_root, data,
			    &grip_area_fops);

}

static void devm_tid_release(struct device *dev, void *res)
{
	struct touch_info_dev **tid = res;

	touch_info_dev_unregister(*tid);
}

static int devm_tid_match(struct device *dev, void *res, void *data)
{
	struct touch_info_dev **this = res, **tid = data;

	return *this == *tid;
}

/**
 * devm_touch_info_dev_allocate: - allocate memory for touch_info_dev
 * @dev:	pointer to the caller device
 * @alloc_ops:	whether allocate memory for touch_info_dev_operations.
 *	if @alloc_ops is %true, the function will allocate memory for
 *	touch_info_dev_operations. if @alloc_ops is %false, it will not.
 */
struct touch_info_dev *devm_touch_info_dev_allocate(struct device *dev,
						    bool alloc_ops)
{
	struct touch_info_dev *tid;
	struct touch_info_dev_operations *tid_ops;

	tid = devm_kzalloc(dev, sizeof(*tid), GFP_KERNEL);
	if (!tid)
		return NULL;

	/**
	 * all other members have been cleared and do not need to
	 * be reinitialized
	 */
	tid->rst_gpio = -1;
	tid->irq_gpio = -1;

	if (!alloc_ops)
		return tid;

	tid_ops = devm_kzalloc(dev, sizeof(*tid_ops), GFP_KERNEL);
	if (!tid_ops) {
		devm_kfree(dev, tid);
		return NULL;
	}
	tid->tid_ops = tid_ops;

	return tid;
}
EXPORT_SYMBOL(devm_touch_info_dev_allocate);

void devm_touch_info_dev_free(struct device *dev, struct touch_info_dev *tid,
			      bool free_ops)
{
	if (free_ops)
		devm_kfree(dev, tid->tid_ops);
	devm_kfree(dev, tid);
}
EXPORT_SYMBOL(devm_touch_info_dev_free);

/**
 * devm_touch_info_dev_register: - create a device for a managed device
 * @dev:  pointer to the caller device
 * @name: name of new device to create
 * @tid:  the device information
 *
 * If an device allocated with this function needs to be freed
 * separately, devm_touch_info_dev_unregister() must be used.
 */
int devm_touch_info_dev_register(struct device *dev, const char *name,
				 struct touch_info_dev *tid)
{
	struct touch_info_dev **dr;
	int ret;

	dr = devres_alloc(devm_tid_release, sizeof(tid), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	ret = touch_info_dev_register(dev, name, tid);
	if (ret) {
		devres_free(dr);
		return ret;
	}

	*dr = tid;
	devres_add(dev, dr);

	create_debug(tid);

	return 0;
}
EXPORT_SYMBOL(devm_touch_info_dev_register);

/**
 * devm_touch_info_dev_unregister: - destroy the device
 * @dev: device to ini_def_name
 * @tid: the device information
 *
 * This function instead of touch_info_dev_unregister() should be used to
 * manually destroy the device allocated with devm_touch_info_dev_register().
 */
void devm_touch_info_dev_unregister(struct device *dev,
				    struct touch_info_dev *tid)
{
	WARN_ON(devres_release(dev, devm_tid_release, devm_tid_match, &tid));
}
EXPORT_SYMBOL(devm_touch_info_dev_unregister);

#define of_property_read_string_and_check(np, tid, prop)		      \
	do {								      \
		if (!tid->prop) {					      \
			if (of_property_read_string(np, "touchpanel,"#prop,   \
						    &tid->prop))	      \
				pr_debug("'%s' property does not exist\n",    \
					 #prop);			      \
		}							      \
	} while (0)

#define of_property_read_bool_and_set(np, tid, prop)			\
	do {								\
		if (of_property_read_bool(np, "touchpanel,"#prop))	\
			tid->prop = true;				\
	} while (0)

static int product_selector_compatible(struct device *dev, const char **compat)
{
	int index = 0;
	ssize_t count;
	struct device_node *np = dev->of_node;
	struct touch_info_dev *tid = dev_to_tid(dev);
	int retry = 0;
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;

	if (!np)
		return -ENODEV;

	count = of_property_count_strings(np, "compatible");
	if (count < 0)
		return count;
	else if (count == 1 || !tid_ops || !tid_ops->product_selector)
		goto got_index;

retry:
	index = tid_ops->product_selector(dev->parent);
	if (index < 0)
		goto err;

	if (index >= count) {
		dev_err(dev, "product_selector may return an invalid value\n");
		return -EPERM;
	}
got_index:
	dev_dbg(dev, "set product information from %s[%d]\n", "compatible",
		index);
	return of_property_read_string_index(np, "compatible", index, compat);
err:
	/**
	 * @vendor and @product is very important! So, we try up to
	 * 3 times when get @index fail.
	 */
	if (retry++ < 3) {
		dev_err(dev, "product selector fail and retry %d times\n",
			retry);
		msleep(20);
		goto retry;
	}

	return index;
}

/**
 * From now on, we support passing parameters via device tree(dts).
 *
 * The valid properties are as follows(e.g.):
 *	compatible = "focal,ft5336", "xxx,yyy";
 *	touchpanel,vendor = "focal";
 *	touchpanel,product = "ft5336";
 *	touchpanel,panel_maker = "boe";
 *	touchpanel,use_dev_path;
 *	touchpanel,fw_name_use_color;
 *	touchpanel,ini_name_use_color;
 *	touchpanel,open_short_not_use_fw;
 *	touchpanel,rst-gpio = <&tlmm 64 0x0>;
 *	touchpanel,irq-gpio = <&tlmm 65 0x0>;
 *	touchpanel,dynamic-gpio = <&tlmm 61 0x0>;
 *	touchpanel,codes = <0 0 0 0 KEY_POWER 0 0 0 0 0 0 0 0 0 0>;
 *
 * Note: Do not use 'touchpanel,use_dev_path', unless you know what this means.
 * For more information about 'touchpanel,codes', see function
 * gesture_report_input_dev_init(). 'touchpanel,vendor' and 'touchpanel,product'
 * are unnecessary. Because we can get this information via 'compatible'
 * property. See function product_selector_compatible(). If
 * 'touchpanel,panel_maker' is not available, we can get panel_maker from
 * lockdowninfo(via tid_ops->get_lockdown_info()).
 */
static int of_touch_info_dev_parse(struct device *dev)
{
	int ret;
	struct device_node *np = dev->of_node;
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (!np) {
		dev_dbg(dev, "%s: device node is not exist\n", __func__);
		return -ENODEV;
	}

	of_property_read_string_and_check(np, tid, vendor);
	of_property_read_string_and_check(np, tid, product);
	of_property_read_string_and_check(np, tid, panel_maker);
	of_property_read_bool_and_set(np, tid, use_dev_path);
	of_property_read_bool_and_set(np, tid, fw_name_use_color);
	of_property_read_bool_and_set(np, tid, ini_name_use_color);
	of_property_read_bool_and_set(np, tid, open_short_not_use_fw);

	if (!gpio_is_valid(tid->rst_gpio))
		tid->rst_gpio = of_get_named_gpio(np, "touchpanel,rst-gpio", 0);
	if (!gpio_is_valid(tid->irq_gpio))
		tid->irq_gpio = of_get_named_gpio(np, "touchpanel,irq-gpio", 0);
	tid->p->dynamic_gpio = of_get_named_gpio(np, "touchpanel,dynamic-gpio",
						 0);

	/**
	 * If 'touchpanel,vendor' and 'touchpanel,product' property is absent.
	 * We can try to get this information from 'compatible'.
	 */
	if (!tid->vendor && !tid->product) {
		const char *find;
		const char *compat;

		ret = product_selector_compatible(dev, &compat);
		if (ret)
			goto err;

		find = strnchr(compat, strlen(compat), ',');
		if (!find) {
			ret = -EINVAL;
			goto err;
		}

		tid->vendor = devm_kzalloc(dev, find - compat + 1, GFP_KERNEL);
		if (!tid->vendor) {
			ret = -ENOMEM;
			goto err;
		}
		memcpy((void *)tid->vendor, compat, find - compat);
		tid->product = find + 1;
	}

	return 0;
err:
	tid->vendor = "none";
	tid->product = "none";
	return ret;
}

static void remove_sysfs_target_links(void *data)
{
	struct device *dev = data;
	const char **names;
	struct kobject **targets;
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;
	size_t nr;
	int i;

	nr = tid_ops->sysfs_create_link_targets(dev->parent, NULL, NULL);
	targets = kzalloc(nr * (sizeof(*targets) + sizeof(*names)), GFP_KERNEL);
	if (!targets)
		return;
	names = (void *)(targets + nr);
	tid_ops->sysfs_create_link_targets(dev->parent, targets, names);

	for (i = 0; i < nr; i++) {
		dev_dbg(dev, "remove sysfs symlink: %s\n", names[i]);
		sysfs_remove_link(targets[i], names[i]);
	}

	kfree(targets);
}

static int sysfs_create_link_targets(struct device *dev)
{
	int ret = -ENOMEM;
	const char **names;
	struct kobject **targets;
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;
	size_t nr;
	int i;

	if (!tid_ops || !tid_ops->sysfs_create_link_targets)
		return 0;

	nr = tid_ops->sysfs_create_link_targets(dev->parent, NULL, NULL);
	if (nr == 0) {
		dev_dbg(dev, "%s: may return an invalid value\n", __func__);
		return 0;
	}
	targets = kzalloc(nr * (sizeof(*targets) + sizeof(*names)), GFP_KERNEL);
	if (!targets)
		goto out;
	names = (void *)(targets + nr);
	tid_ops->sysfs_create_link_targets(dev->parent, targets, names);

	for (i = 0; i < nr; i++) {
		const char *from = kobject_get_path(targets[i], GFP_KERNEL);
		const char *to = kobject_get_path(&dev->kobj, GFP_KERNEL);

		dev_info(dev, "create sysfs symlink: %s/%s as %s\n",
			 from ? : "(null)", names[i], to ? : "(null)");
		kfree(from);
		kfree(to);
		ret = sysfs_create_link(targets[i], &dev->kobj, names[i]);
		if (ret) {
			dev_err(dev, "failed to create link: %s\n", names[i]);
			goto remove_links;
		}
	}
	ret = devm_add_action(dev, remove_sysfs_target_links, dev);
	if (ret) {
		dev_err(dev, "%s: failed to add action\n", __func__);
		goto remove_links;
	}
	i = 0;

remove_links:
	while (--i >= 0)
		sysfs_remove_link(targets[i], names[i]);

	kfree(targets);
out:
	return ret;
}

static void remove_proc_target_links(void *data)
{
	struct device *dev = data;
	const char **names;
	struct proc_dir_entry **targets;
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;
	size_t nr;
	int i;

	nr = tid_ops->proc_create_link_targets(dev->parent, NULL, NULL);
	targets = kzalloc(nr * (sizeof(*targets) + sizeof(*names)), GFP_KERNEL);
	if (!targets)
		return;
	names = (void *)(targets + nr);
	tid_ops->proc_create_link_targets(dev->parent, targets, names);

	for (i = 0; i < nr; i++) {
		dev_dbg(dev, "remove proc symlink: %s\n", names[i]);
		remove_proc_entry(names[i], targets[i]);
	}

	kfree(targets);
}

static int proc_create_link_targets(struct device *dev)
{
	const char **names;
	struct proc_dir_entry **targets;
	struct touch_info_dev_operations *tid_ops = dev_to_tid(dev)->tid_ops;
	size_t nr;
	int i;
	char path[64];
	int ret;

	if (!tid_ops || !tid_ops->proc_create_link_targets)
		return 0;

	nr = tid_ops->proc_create_link_targets(dev->parent, NULL, NULL);
	if (nr == 0) {
		dev_dbg(dev, "%s: may return an invalid value\n", __func__);
		return 0;
	}
	targets = kzalloc(nr * (sizeof(*targets) + sizeof(*names)), GFP_KERNEL);
	if (!targets)
		return -ENOMEM;
	names = (void *)(targets + nr);
	tid_ops->proc_create_link_targets(dev->parent, targets, names);

	snprintf(path, ARRAY_SIZE(path), "/sys/class/%s/%s",
		 TOUCHSCREEN_CLASS_NAME, dev_name(dev));
	for (i = 0; i < nr; i++) {
		struct proc_dir_entry *ent;

		dev_info(dev, "create proc symlink: %s as %s\n", names[i],
			 path);
		ent = proc_symlink(names[i], targets[i], path);
		if (!ent) {
			dev_err(dev, "failed to create link: %s\n", names[i]);
			ret = -EINVAL;
			goto remove_links;
		}
	}
	ret = devm_add_action(dev, remove_proc_target_links, dev);
	if (ret) {
		dev_err(dev, "%s: failed to add action\n", __func__);
		goto remove_links;
	}
	i = 0;

remove_links:
	while (--i >= 0)
		remove_proc_entry(names[i], targets[i]);
	kfree(targets);

	return ret;
}

static size_t sysfs_create_links_dfl(struct device *dev,
				     struct kobject **targets,
				     const char **names)
{
	return 0;
}

static size_t proc_create_links_dfl(struct device *dev,
				    struct proc_dir_entry **targets,
				    const char **names)
{
	if (!targets && !names)
		return 1;

	targets[0] = NULL;
	names[0] = DEFAULT_DEVICE_NAME;

	return 1;
}

static void remove_fb_notifier(void *data)
{
	fb_notifier_remove(data);
}

static void tid_release(struct device *dev)
{
	dev_dbg(dev, "device: '%s' remove\n", dev_name(dev));
}

/**
 * touch_info_dev_register: - create a device with some special file of sysfs
 * @dev:  pointer to the caller device
 * @name: name of new device to create
 * @tid:  the device information
 *
 * If the @name is NULL, the name of created device will be "touchpanel".
 * You should call the touch_info_dev_unregister() to destroy the device which
 * is created by touch_info_dev_register().
 */
int touch_info_dev_register(struct device *dev, const char *name,
			    struct touch_info_dev *tid)
{
	int ret;
	struct device *device;
	const char *dev_name = name ? : DEFAULT_DEVICE_NAME;
	struct tid_private *p;
	struct touch_info_dev_operations *tid_ops;

	BUG_ON(!tid || !dev);
	if (find_tid_by_name(dev_name)) {
		pr_err("'%s' is already registered\n", dev_name);
		return -EEXIST;
	}

	pr_debug("device: '%s' register\n", dev_name);
	device = tid_to_dev(tid);
	memset(device, 0, sizeof(*device));
	device_initialize(device);
	device->devt	= MKDEV(0, 0);
	device->class	= &touchscreen_class;
	device->parent	= dev;
	device->release	= tid_release;
	device->of_node	= dev->of_node;
	dev_set_drvdata(device, tid);
	ret = dev_set_name(device, "%s", dev_name);
	if (ret)
		goto error;
	ret = device_add(device);
	if (ret)
		goto error;

	p = devm_kzalloc(device, sizeof(*p), GFP_KERNEL);
	if (!p) {
		ret = -ENOMEM;
		goto unregister_dev;
	}
	tid->p = p;
	p->dev = device;
	p->dynamic_gpio = -1;
	mutex_init(&p->reg_mutex);

	tid_ops = tid->tid_ops;
	if (tid_ops && !tid_ops->proc_create_link_targets)
		tid_ops->proc_create_link_targets = proc_create_links_dfl;
	if (tid_ops && !tid_ops->sysfs_create_link_targets)
		tid_ops->sysfs_create_link_targets = sysfs_create_links_dfl;

	ret = gesture_report_init(device);
	if (ret)
		goto unregister_dev;

	fb_notifier_init(device);
	ret = devm_add_action(device, remove_fb_notifier, device);
	if (ret) {
		fb_notifier_remove(dev);
		dev_err(device, "failed to add action: %s\n", __func__);
		goto unregister_dev;
	}
	ret = sysfs_create_link_targets(device);
	if (ret)
		goto unregister_dev;
	ret = proc_create_link_targets(device);
	if (ret)
		goto unregister_dev;
	touch_proc_add_device(device);
	of_touch_info_dev_parse(device);

	return 0;
unregister_dev:
	devres_release_all(device);
	device_del(device);
error:
	put_device(device);
	return ret;
}
EXPORT_SYMBOL(touch_info_dev_register);

/**
 * touch_info_dev_unregister: - destroy the device which is created
 * via touch_info_dev_register()
 * @tid:  the device information
 *
 * You should call the touch_info_dev_unregister() to destroy the device
 * which is created via touch_info_dev_register().
 */
void touch_info_dev_unregister(struct touch_info_dev *tid)
{
	struct device *dev = tid_to_dev(tid);

	dev_dbg(dev, "device: '%s' unregister\n", dev_name(dev));
	device_init_wakeup(dev, false);
	touch_proc_del_device(dev);
	devres_release_all(dev);
	device_unregister(dev);
}
EXPORT_SYMBOL(touch_info_dev_unregister);

/**
 * tid_hardware_info_get: - get hardware info and print it to the buf
 * @buf:  the buffer to store hardware info
 * @size: the buffer size
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is == 0 the function returns 0. If something
 * error, it return errno.
 */
int tid_hardware_info_get(char *buf, size_t size)
{
	int ret;
	const char *color;
	unsigned int minor;
	struct device *dev;
	struct touch_info_dev *tid;

	if (!buf || !size)
		return -EINVAL;
	tid = find_default_tid();
	if (!tid)
		return -ENODEV;
	dev = tid_to_dev(tid);
	get_device(dev);

	ret = get_version(dev, NULL, &minor);
	if (ret) {
		dev_err(dev, "get version fail and set version 0\n");
		minor = 0;
	}

	get_panel_maker(dev);
	color = get_panel_color(dev);

	ret = scnprintf(buf, size,
			color ? "%s,%s,fw:0x%02X,%s" : "%s,%s,fw:0x%02X",
			tid->panel_maker ? : "none", tid->product, minor,
			color);
	dev_info(dev, "hardware info is '%s'\n", buf);
	put_device(dev);

	return ret;
}
EXPORT_SYMBOL(tid_hardware_info_get);

/**
 * In tid_upgrade_firmware_nowait(), I want to use a local variable to store
 * firmware name. But it does not work well when the kernel version below 4.4.
 * If you use the linux-4.15, you can just do this. Because the
 * request_firmware_nowait() will request memory to store firmware name. Details
 * can compare the request_firmware_nowait() function between linux-4.4 and
 * linux-4.15.
 */
struct firmware_context {
	struct device *dev;
	char firmware_name[64];
};

static void firmware_callback(const struct firmware *fw, void *context)
{
	int ret;
	unsigned int minor_new = 0, minor_old = 0;
	struct firmware_context *fw_context = context;
	struct device *dev = fw_context->dev;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	bool use_color = tid->fw_name_use_color;

	/**
	 * If we request firmware fail, we can retry once.
	 */
	if (!fw || !fw->data) {
		int len;
		char name[64] = { 0 };

		dev_err(dev, "load firmware '%s' fail and retry\n",
			fw_context->firmware_name);
		tid->fw_name_use_color = !use_color;
		len = get_firmware_name(dev, name, ARRAY_SIZE(name));
		if (len > ARRAY_SIZE(name) - 1) {
			dev_err(dev,
				"get firmware name fail, the buf size is too small\n");
			goto out;
		}

		if (!strcmp(name, fw_context->firmware_name))
			goto out;
		dev_dbg(dev, "retry firmware name is '%s'\n", name);

		memcpy(fw_context->firmware_name, name, ARRAY_SIZE(name));
		if (request_firmware(&fw, fw_context->firmware_name, dev))
			goto out;
		use_color = !use_color;
	}

	if (!tid_ops || !tid_ops->firmware_upgrade)
		goto out;

	if (cmpxchg_acquire(&tid->p->is_upgrading_firmware, 0, 1) != 0) {
		dev_info(dev, "is upgrading firmware, please wait\n");
		goto out;
	}

	ret = get_version(dev, NULL, &minor_old);
	if (ret)
		dev_err(dev, "%s: get firmware version fail", __func__);
	else
		dev_dbg(dev,
			"before upgrade firmware, the version is: 0x%02X\n",
			minor_old);

	ret = tid_ops->firmware_upgrade(dev->parent, fw, false);
	if (ret) {
		dev_err(dev, "upgrade firmware fail with errno: %d\n", ret);
		goto reset_fw;
	}

	ret = get_version(dev, NULL, &minor_new);
	if (ret) {
		dev_err(dev, "%s: get firmware version fail", __func__);
		goto reset_fw;
	}
	if (minor_new > minor_old)
		dev_info(dev,
			 "upgrade firmware success, the version is: 0x%02X\n",
			 minor_new);
	else
		dev_dbg(dev, "no need to upgrade firmware\n");

reset_fw:
	smp_store_release(&tid->p->is_upgrading_firmware, 0);
out:
	tid->fw_name_use_color = use_color;
	release_firmware(fw);
	kfree(fw_context);
	/* matches tid_upgrade_firmware_nowait() */
	put_device(dev);
}

static size_t get_firmware_name(struct device *dev, char *buf, size_t size)
{
	const char *color = NULL;
	struct touch_info_dev *tid = dev_to_tid(dev);
	int hw_id;
	size_t len;

	get_panel_maker(dev);
	if (tid->fw_name_use_color)
		color = get_panel_color(dev);
	hw_id = get_hardware_id(dev);

	if (hw_id)
		len = snprintf(buf, size,
			       color ? "%s-%s-%s-h%d-%s.img" :
			       "%s-%s-%s-h%d.img",
			       tid->vendor, tid->product,
			       tid->panel_maker ? : "none", hw_id, color);
	else
		len = snprintf(buf, size,
			       color ? "%s-%s-%s-%s.img" : "%s-%s-%s.img",
			       tid->vendor, tid->product,
			       tid->panel_maker ? : "none", color);

	return len;
}

/**
 * tid_upgrade_firmware_nowait: - asynchronous version of request_firmware
 * @tid: struct touch_info_dev
 *
 * Notice: If disable lockdown info interface, the firmware name is
 * 'vendor-product-panelmaker.img'. Otherwise, the firmware name is
 * 'vendor-product-panelmaker-panelcolor.img'. if @tid->fw_name_use_color
 * is set.
 */
int tid_upgrade_firmware_nowait(struct touch_info_dev *tid)
{
	int ret;
	struct device *dev = tid_to_dev(tid);
	struct firmware_context *fw_context;

	if (unlikely(!device_is_registered(dev))) {
		pr_err("device is not registered\n");
		return -ENODEV;
	}

	fw_context = kzalloc(sizeof(*fw_context), GFP_KERNEL);
	if (!fw_context)
		return -ENOMEM;

	get_device(dev);
	fw_context->dev = dev;

	ret = get_firmware_name(dev, fw_context->firmware_name,
				ARRAY_SIZE(fw_context->firmware_name));
	if (ret > ARRAY_SIZE(fw_context->firmware_name) - 1) {
		dev_err(dev,
			"get firmware name fail, the buf size is too small\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_dbg(dev, "firmware name is '%s'\n", fw_context->firmware_name);

	ret = request_firmware_nowait(THIS_MODULE, true,
				      fw_context->firmware_name, dev,
				      GFP_KERNEL, fw_context,
				      firmware_callback);
	if (ret)
		goto err;

	return 0;
err:
	kfree(fw_context);
	put_device(dev);
	return ret;
}
EXPORT_SYMBOL(tid_upgrade_firmware_nowait);

int tid_upgrade_firmware_opt(struct touch_info_dev *tid, bool direct,
			     bool force)
{
	int ret;
	struct device *dev = tid_to_dev(tid);
	char firmware_name[64] = { 0 };
	const struct firmware *fw;
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	unsigned int version;

	if (unlikely(!device_is_registered(dev))) {
		pr_err("device is not registered\n");
		return -ENODEV;
	}

	if (!tid_ops || !tid_ops->firmware_upgrade)
		return -ENODEV;

	get_device(dev);
	if (cmpxchg_acquire(&tid->p->is_upgrading_firmware, 0, 1) != 0) {
		dev_info(dev, "is upgrading firmware, please wait\n");
		ret = -EBUSY;
		goto put_dev;
	}

	ret = get_firmware_name(dev, firmware_name, ARRAY_SIZE(firmware_name));
	if (ret > ARRAY_SIZE(firmware_name) - 1) {
		dev_err(dev,
			"get firmware name fail, the buf size is too small\n");
		ret = -ENOMEM;
		goto out;
	}
	dev_dbg(dev, "firmware name is '%s'\n", firmware_name);

	if (direct)
		ret = request_firmware_direct(&fw, firmware_name, dev);
	else
		ret = request_firmware(&fw, firmware_name, dev);
	if (ret)
		goto out;
	ret = tid_ops->firmware_upgrade(dev->parent, fw, force);
	release_firmware(fw);
	if (ret) {
		dev_err(dev, "upgrade firmware fail with errno: %d\n", ret);
		goto out;
	}

	ret = get_version(dev, NULL, &version);
	if (ret) {
		dev_err(dev, "%s: get firmware version fail", __func__);
		ret = 0;
		goto out;
	}
	if (force)
		dev_info(dev, "upgrade firmware success\n");
	dev_info(dev, "the firmware version is: 0x%02X\n", version);
out:
	smp_store_release(&tid->p->is_upgrading_firmware, 0);
put_dev:
	put_device(dev);

	return ret;
}
EXPORT_SYMBOL(tid_upgrade_firmware_opt);

static ssize_t regulators_get(struct device *dev, struct regulator **regs)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	size_t count = 0;

	if (!np)
		return -EINVAL;

	for_each_property_of_node(np, prop) {
		char *find;
		char name[32];
		int i = count;
		size_t size;

		find = strnstr(prop->name, "-supply", strlen(prop->name));
		if (!find || strcmp(find, "-supply"))
			continue;
		count++;
		if (!regs)
			continue;
		size = find - prop->name;
		if (size > sizeof(name) - 1)
			size = sizeof(name) - 1;
		memcpy(name, prop->name, size);
		name[size] = '\0';
		dev_dbg(dev, "regulator name is '%s-supply'\n", name);
		regs[i] = devm_regulator_get(dev, name);
		if (IS_ERR(regs[i])) {
			long err = PTR_ERR(regs[i]);

			dev_err(dev, "get regulator(%s) fail\n", name);
			while (--i >= 0)
				devm_regulator_put(regs[i]);
			return err;
		}
	}

	return count;
}

int tid_regulators_enable_opt(struct touch_info_dev *tid, bool enable)
{
	int i;
	int ret = 0;
	struct device *dev;
	struct tid_private *p;
	struct regulator **regs;
	int reg_count;

	if (!tid)
		return -EINVAL;

	dev = tid_to_dev(tid);
	if (unlikely(!device_is_registered(dev))) {
		pr_err("device is not registered\n");
		return -ENODEV;
	}

	get_device(dev);
	p = tid->p;
	mutex_lock(&p->reg_mutex);
	if (p->regs_enabled == enable)
		goto out;

	regs = p->regs;
	reg_count = p->reg_count;

	if (unlikely(!reg_count)) {
		ret = regulators_get(dev, NULL);
		if (ret <= 0)
			goto out;

		regs = devm_kzalloc(dev, ret * sizeof(*regs), GFP_KERNEL);
		if (!regs) {
			ret = -ENOMEM;
			goto out;
		}

		ret = regulators_get(dev, regs);
		if (ret < 0) {
			devm_kfree(dev, regs);
			goto out;
		}
		dev_dbg(dev, "regulator count is: %d\n", ret);
		p->reg_count = ret;
		reg_count = ret;
		p->regs = regs;
	}

	if (!enable) {
		i = reg_count;
		goto disable_regs;
	}

	for (i = 0; i < reg_count; i++) {
		ret = regulator_enable(regs[i]);
		if (ret) {
			dev_err(dev,
				"enable regulator[%d] fail with errno: %d\n", i,
				ret);
			enable = false;
			goto disable_regs;
		}
	}
	i = 0;

disable_regs:
	while (--i >= 0)
		regulator_disable(regs[i]);
	p->regs_enabled = enable;
out:
	mutex_unlock(&p->reg_mutex);
	put_device(dev);

	return ret;
}
EXPORT_SYMBOL(tid_regulators_enable_opt);

/**
 * tid_panel_maker: - get panel maker of touchscreen
 */
const char *tid_panel_maker(void)
{
	struct touch_info_dev *tid = find_default_tid();

	return tid ? tid->panel_maker : NULL;
}
EXPORT_SYMBOL(tid_panel_maker);

/**
 * tid_panel_color: - get panel color of touchscreen
 */
const char *tid_panel_color(void)
{
	struct touch_info_dev *tid = find_default_tid();

	return tid ? tid->panel_color : NULL;
}
EXPORT_SYMBOL(tid_panel_color);

static int touch_info_dev_init(void)
{
	class_register(&touchscreen_class);
	create_proc();
	pr_debug("touch info interface ready\n");

	return 0;
}
subsys_initcall(touch_info_dev_init);

MODULE_AUTHOR("smcdef <songmuchun@wingtech.com>");
MODULE_LICENSE("GPL v2");

/**
 * Copyright (C) 2018 Fourier Semiconductor Inc. All rights reserved.
 * 2019-04-17 File created.
 */

#define pr_fmt(fmt) "%s:%d: " fmt, __func__, __LINE__
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#include "fs15xx.h"

static fs15xx_dev_t *g_fs15xx_dev = NULL;
static atomic_t g_fs15xx_mode;
static atomic_t g_fs15xx_switch;

void fs15xx_delay_ms(uint32_t delay_ms)
{
	if (delay_ms == 0) return;
	usleep_range(delay_ms * 1000, delay_ms * 1000 + 1);
}

#ifdef FS15XX_PINCTRL_SUPPORT
static int fs15xx_pinctrl_init(struct platform_device *pdev, fs15xx_dev_t *fs15xx)
{
	struct pinctrl *ctrl;
	struct pinctrl_state *state;
	char *state_name;
	int ret = 0;

	if (!fs15xx)
		return -EINVAL;

	ctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ctrl)) {
		dev_err(&pdev->dev, "not found pinctrl!\n");
		ret = PTR_ERR(fs15xx->pinctrl);
		fs15xx->pinctrl = NULL;
		return ret;
	}
	fs15xx->pinctrl = ctrl;

	state_name = "fs15xx_id_default";
	state = pinctrl_lookup_state(ctrl, state_name);
	if (IS_ERR(state)) {
		ret = PTR_ERR(state); // TODO
		pr_err("lookup state: %s failed\n", state_name);
	} else {
		pinctrl_select_state(fs15xx->pinctrl, state);
		fs15xx->fs15xx_id_default = state;
	}

	state_name = "fs15xx_cmd_default";
	state = pinctrl_lookup_state(ctrl, state_name);
	if (IS_ERR(state)) {
		ret = PTR_ERR(state);
		pr_err("lookup state: %s failed, ret: %d\n", state_name, ret);
	} else {
		pinctrl_select_state(fs15xx->pinctrl, state);
		fs15xx->fs15xx_cmd_default = state;
	}

	if (fs15xx->dev_type == FS15XX_DEV_1910) {
		pr_info("spc1910 done");
		return ret;
	}

	state_name = "fs15xx_mod_default";
	state = pinctrl_lookup_state(ctrl, state_name);
	if (IS_ERR(state)) {
		ret = PTR_ERR(state); // TODO if use mod pin, open this line
		pr_err("lookup state: %s failed\n", state_name);
	} else {
		pinctrl_select_state(fs15xx->pinctrl, state);
		fs15xx->fs15xx_mod_default = state;
	}
	pr_info("fs15xx done");

	return ret;
}
#endif

#ifdef CONFIG_OF
static int fs15xx_of_init(struct platform_device *pdev, fs15xx_dev_t *fs15xx)
{
	struct device_node *np = pdev->dev.of_node;
	u32 gpio;
	int ret = 0;

	if (!fs15xx || !np)
		return -EINVAL;

	if (of_property_read_u32(np, "fsm,dev-type", &fs15xx->dev_type)) {
		pr_info("fsm,dev-type property missing in DT");
		fs15xx->dev_type = FS15XX_DEV_1910;
	}
	// of_property_read_u32(np, "fsm,id-gpio", (u32 *)&gpio);
	gpio = of_get_named_gpio(np, "fsm,id-gpio", 0);
	if (gpio_is_valid(gpio)) {
		pr_info("gpio_id: %d\n", gpio);
		fs15xx->gpio_id = gpio;
		ret |= devm_gpio_request(&pdev->dev, gpio, "fs15xx id");
		if (ret)
			pr_err("request gpio fail:%d\n", gpio);
		ret |= gpio_direction_output(gpio, 0);
	}
	else {
		pr_err("invalid gpio id: %d\n", gpio);
		fs15xx->gpio_id = -1;
	}

	gpio = of_get_named_gpio(np, "fsm,cmd-gpio", 0);
	if (gpio_is_valid(gpio)) {
		pr_info("gpio_cmd: %d\n", gpio);
		fs15xx->gpio_cmd = gpio;
		// ret = gpio_request(gpio, "fs15xx cmd");
		ret |= devm_gpio_request(&pdev->dev, gpio, "fs15xx cmd");
		if (ret)
			pr_err("request gpio fail:%d\n", gpio);
		ret |= gpio_direction_output(gpio, 0);
	}
	else {
		pr_err("invalid gpio cmd: %d\n", gpio);
		fs15xx->gpio_cmd = -1;
		return -EINVAL;
	}
	if (fs15xx->dev_type == FS15XX_DEV_1910) {
		return 0;
	}
	gpio = of_get_named_gpio(np, "fsm,mod-gpio", 0);
	if (gpio_is_valid(gpio)) {
		pr_info("gpio_mod: %d\n", gpio);
		fs15xx->gpio_mod = gpio;
		ret |= devm_gpio_request(&pdev->dev, gpio, "fs15xx mod");
		if (ret)
			pr_err("request gpio fail:%d\n", gpio);
		ret |= gpio_direction_output(gpio, 0); // boost mode
	}
	else {
		pr_err("invalid gpio mod: %d\n", gpio);
		fs15xx->gpio_mod = -1;
		// return -EINVAL; // TODO
	}

	return ret;
}
#endif

static __inline int fs15xx_ctrl_pin(fs15xx_dev_t *fs15xx, int on)
{
	int ret = 0;

	if (fs15xx == NULL)
		return -EINVAL;

	on = !!on;
	if (gpio_is_valid(fs15xx->gpio_cmd)) {
		/* if gpio under a gpio controller, use gpio_set_value_cansleep */
		// gpio_set_value_cansleep(fs15xx->gpio_cmd, on);
		gpio_set_value(fs15xx->gpio_cmd, on);
	}
	else {
		ret = -EINVAL;
	}
	fs15xx->last_time = fs15xx->cur_time;
	fs15xx->cur_time = ktime_get_boottime();

	return ret;
}

static __inline int fs15xx_shutdown(fs15xx_dev_t *fs15xx)
{
	int ret;

	if (fs15xx == NULL)
		return -EINVAL;

	fs15xx->cur_time = ktime_get_boottime();
	gpio_direction_output(fs15xx->gpio_cmd, 0);
	ret = fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_LOW);
	fs15xx_delay_ms(5); // 5ms+
	fs15xx->fs15xx_mode = FS15XX_OFF_MODE;

	return ret;
}

static __inline int check_intervel_time(fs15xx_dev_t *fs15xx,
				s64 intervel_min, s64 intervel_max)
{
	u64 delta_time;

	delta_time = ktime_to_ns(ktime_sub(fs15xx->cur_time,
			fs15xx->last_time));
	do_div(delta_time, 1000); // ns -> us
	if (delta_time < intervel_min || delta_time >= intervel_max) {
		fs15xx->fs15xx_timeout = true;
		return -EINVAL;
	}

	return 0;
}

static __inline int fs15xx_send_pulse(fs15xx_dev_t *fs15xx, bool polar)
{
	int ret;

	if (fs15xx == NULL)
		return -EINVAL;

	ret = fs15xx_ctrl_pin(fs15xx, polar);
	ret |= check_intervel_time(fs15xx, 0, 130);
	udelay(FS15XX_PULSE_DELAY_US); // < 140us
	ret |= fs15xx_ctrl_pin(fs15xx, !polar);
	ret |= check_intervel_time(fs15xx, 0, 130);
	udelay(FS15XX_PULSE_DELAY_US); // < 140us

	return ret;
}

static __inline int spc1910_send_hdr(fs15xx_dev_t *fs15xx)
{
	int count = SPC1910_HDR_PULSES;
	ktime_t start_time;
	int ret = 0;

	if (fs15xx == NULL)
		return -EINVAL;

	fs15xx->fs15xx_timeout = false;
	// reset cur time
	start_time = ktime_get_boottime();
	fs15xx->cur_time = start_time;
	while (count > 0) {
		ret |= fs15xx_send_pulse(fs15xx, 1);
		count--;
	}
	udelay(60); // 50us~190us
	fs15xx->last_time = fs15xx->cur_time;
	fs15xx->cur_time = ktime_get_boottime();
	ret |= check_intervel_time(fs15xx, 55, 180);

	// check header total time, need total_time < 1ms
	fs15xx->last_time = start_time;
	fs15xx->cur_time = ktime_get_boottime();
	ret |= check_intervel_time(fs15xx, 0, 950);
	if (ret) {
		fs15xx->fs15xx_timeout = true;
		return -EINVAL;
	}

	return ret;
}

static __inline int spc19xx_mode_to_count(int mode)
{
	int count = 0;

	switch (mode) {
	case 1: // RCV1
		count = 8;
		break;
	case 2: // RCV2
		count = 9;
		break;
	case 4: // SPK1
		count = 1;
		break;
	case 5: // SPK2
		count = 10;
		break;
	default:
		pr_err("unsupport mode:%d", mode);
		break;
	}

	return count;
}

int spc19xx_set_mode(int mode)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	unsigned long flags;
	int count;
	int retry = 0;
	int ret;

	if (fs15xx == NULL)
		return -EINVAL;
	if (!gpio_is_valid(fs15xx->gpio_cmd)) {
		pr_err("invalid gpio\n");
		return -EINVAL;
	}
	pr_info("mode: %d-->%d\n", fs15xx->fs15xx_mode, mode);
	if (fs15xx->fs15xx_mode == mode) {
		// the same mode, not to switch again
		return 0;
	}
	// switch mode online, need shut down pa firstly
	fs15xx_shutdown(fs15xx);
	if (mode == FS15XX_OFF_MODE || mode == 3 || mode == 6) {
		// not support RCV3 and SPK3 Mode
		fs15xx->fs15xx_mode = mode;
		return 0;
	}
	// enable pa into work mode
	do {
		// make sure idle mode: gpio output low
		gpio_direction_output(fs15xx->gpio_cmd, 0);
		fs15xx->fs15xx_timeout = false;
		spin_lock_irqsave(&fs15xx->fs15xx_lock, flags);
		// 1. send header
		ret = spc1910_send_hdr(fs15xx);
		if (ret || fs15xx->fs15xx_timeout) {
			spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
			continue;
		}
		// 2. send mode
		count = spc19xx_mode_to_count(mode);
		while (count > 0) { // count of pulse
			ret |= fs15xx_send_pulse(fs15xx, 1);
			count--;
		}
		if (ret || fs15xx->fs15xx_timeout) {
			spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
			continue;
		}
		udelay(600); // 500us~2ms
		// 3. pull up gpio and delay, enable pa
		ret |= fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_HIGH);
		ret |= check_intervel_time(fs15xx, 500, 1950);
		spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
		if (ret || fs15xx->fs15xx_timeout) {
			ret |= fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_LOW);
			udelay(320); // shutdown and try again
			continue;
		}
		udelay(300); // pull up gpio > 220us
		fs15xx->fs15xx_mode = mode;
		break;
	} while (retry++ < FS15XX_RETRY);
	if (retry > 0)
		pr_info("retry %d times\n", retry);

	return ret;
}

int spc19xx_set_mode_simple(int mode)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	int count;

	if (fs15xx == NULL)
		return -EINVAL;
	if (!gpio_is_valid(fs15xx->gpio_cmd)) {
		pr_err("invalid gpio\n");
		return -EINVAL;
	}
	pr_info("mode: %d-->%d\n", fs15xx->fs15xx_mode, mode);
	if (fs15xx->fs15xx_mode == mode) {
		// the same mode, not to switch again
		return 0;
	}
	gpio_direction_output(fs15xx->gpio_cmd, 0);
	gpio_set_value(fs15xx->gpio_cmd, 0);
	fs15xx_delay_ms(5); // 5ms+
	if (mode == FS15XX_OFF_MODE || mode == 3 || mode == 6) {
		// not support RCV3 and SPK3 Mode
		fs15xx->fs15xx_mode = mode;
		return 0;
	}

	// enable pa into work mode
	// make sure idle mode: gpio output low
	// 1. send header
	count = SPC1910_HDR_PULSES;
	while (count > 0) {
		gpio_set_value(fs15xx->gpio_cmd, 1);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		gpio_set_value(fs15xx->gpio_cmd, 0);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		count--;
	}
	udelay(60); // 50us~190us
	// 2. send mode
	count = spc19xx_mode_to_count(mode);
	while (count > 0) { // count of pulse
		gpio_set_value(fs15xx->gpio_cmd, 1);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		gpio_set_value(fs15xx->gpio_cmd, 0);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		count--;
	}
	udelay(600); // 500us~2ms
	// 3. pull up gpio and delay, enable pa
	gpio_set_value(fs15xx->gpio_cmd, 1);
	udelay(300); // pull up gpio > 220us
	fs15xx->fs15xx_mode = mode;

	return 0;
}

int fs15xx_set_mode(int mode)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	unsigned long flags;
	int count;
	int retry = 0;
	int ret;

	if (fs15xx == NULL)
		return -EINVAL;
	if (!gpio_is_valid(fs15xx->gpio_cmd)) {
		pr_err("invalid gpio\n");
		return -EINVAL;
	}
	pr_info("mode: %d-->%d\n", fs15xx->fs15xx_mode, mode);
	if (fs15xx->fs15xx_mode == mode) {
		// the same mode, not to switch again
		return 0;
	}
	// switch mode online, need shut down pa firstly
	fs15xx_shutdown(fs15xx);
	if (mode == FS15XX_OFF_MODE) {
		fs15xx->fs15xx_mode = mode;
		return 0;
	}
	// enable pa into work mode
	do {
		// make sure idle mode: gpio output low
		gpio_direction_output(fs15xx->gpio_cmd, 0);
		fs15xx->fs15xx_timeout = false;
		spin_lock_irqsave(&fs15xx->fs15xx_lock, flags);
		// 1. send header
		ret = fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_HIGH);
		fs15xx->last_time = fs15xx->cur_time;
		udelay(300); // 200 < t < 1000
		fs15xx->cur_time = ktime_get_boottime();
		ret |= check_intervel_time(fs15xx, 200, 1000);
		// 2. send mode
		count = mode;
		while (count > 0) { // count of pulse
			ret |= fs15xx_send_pulse(fs15xx, 0);
			count--;
		}
		spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
		if (ret || fs15xx->fs15xx_timeout) {
			ret |= fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_LOW);
			udelay(320); // shutdown and try again
			continue;
		}
		udelay(300); // pull up gpio > 220us
		fs15xx->fs15xx_mode = mode;
		break;
	} while (retry++ < FS15XX_RETRY);
	if (retry > 0)
		pr_info("retry %d times\n", retry);

	return ret;
}

/* Add fs1512 & x102 & xx318/xx358 pa type check logic : B2 & C3 */
#ifdef FS15XX_MULTI_TYPE_CHECK
int x102_xx318_set_mode(int mode)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	//unsigned long flags;
	int count;

	if (fs15xx == NULL)
		return -EINVAL;
	if (!gpio_is_valid(fs15xx->gpio_cmd)) {
		pr_err("invalid gpio\n");
		return -EINVAL;
	}
	pr_info("mode: %d-->%d\n", fs15xx->fs15xx_mode, mode);
	if (fs15xx->fs15xx_mode == mode) {
		// the same mode, not to switch again
		return 0;
	}
	gpio_direction_output(fs15xx->gpio_cmd, 0);
	//spin_lock_irqsave(&fs15xx->fs15xx_lock, flags);
	gpio_set_value(fs15xx->gpio_cmd, 0);
	fs15xx_delay_ms(5);
	if (mode == FS15XX_OFF_MODE) {
		fs15xx->fs15xx_mode = mode;
		//spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
		return 0;
	}

	// enable pa into work mode
	gpio_set_value(fs15xx->gpio_cmd, 1);
	udelay(2);
	// send mode
	count = mode - 1;
	while (count > 0) { // count of pulse
		gpio_set_value(fs15xx->gpio_cmd, 0);
		udelay(2);
		gpio_set_value(fs15xx->gpio_cmd, 1);
		udelay(2);
		count--;
	}
	//spin_unlock_irqrestore(&fs15xx->fs15xx_lock, flags);
	udelay(300);
	fs15xx->fs15xx_mode = mode;

	return 0;
}
#endif

int fs15xx_set_mode_simple(int mode)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	int count;

	if (fs15xx == NULL)
		return -EINVAL;
	if (!gpio_is_valid(fs15xx->gpio_cmd)) {
		pr_err("invalid gpio\n");
		return -EINVAL;
	}
	pr_info("mode: %d-->%d\n", fs15xx->fs15xx_mode, mode);
	if (fs15xx->fs15xx_mode == mode) {
		// the same mode, not to switch again
		return 0;
	}
	gpio_direction_output(fs15xx->gpio_cmd, 0);
	gpio_set_value(fs15xx->gpio_cmd, 0);
	fs15xx_delay_ms(5); // 5ms+
	if (mode == FS15XX_OFF_MODE) {
		fs15xx->fs15xx_mode = mode;
		return 0;
	}

	// enable pa into work mode
	// 1. send header
	gpio_set_value(fs15xx->gpio_cmd, 1);
	udelay(300); // 200us < t < 1000us
	// 2. send mode
	count = mode;
	while (count > 0) { // count of pulse
		gpio_set_value(fs15xx->gpio_cmd, 0);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		gpio_set_value(fs15xx->gpio_cmd, 1);
		udelay(FS15XX_PULSE_DELAY_US); // < 140us
		count--;
	}
	udelay(300); // pull up gpio > 220us
	fs15xx->fs15xx_mode = mode;

	return 0;
}

void fs15xx_boost_mode(bool on)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	bool state;

	if (fs15xx == NULL)
		return;
	if (!gpio_is_valid(fs15xx->gpio_mod)) {
		pr_err("invalid gpio\n");
		return;
	}
	state = !on;
	pr_debug("boost %s\n", on ? "On" : "Off");
	if (gpio_is_valid(fs15xx->gpio_mod)) {
		/* if gpio under a gpio controller, use gpio_set_value_cansleep */
		// gpio_set_value_cansleep(fs15xx->gpio_mod, state);
		gpio_set_value(fs15xx->gpio_mod, state);
	}
}
EXPORT_SYMBOL(fs15xx_boost_mode);

#ifdef FS15XX_USE_HRTIMER
static enum hrtimer_restart fs15xx_timer_callback(struct hrtimer *timer)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;

	if (!fs15xx) {
		return HRTIMER_NORESTART;
	}
	if (!atomic_read(&fs15xx->running)) {
		return HRTIMER_NORESTART;
	}
	schedule_work(&fs15xx->monitor_work);

	return HRTIMER_NORESTART;
}

static void fs15xx_monitor(struct work_struct *work)
{
	// check boost or not
	// fs15xx_set_boost_mode();
	fs15xx_set_timer(true);
}

int fs15xx_set_timer(bool enable)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;

	if (!fs15xx) {
		return -EINVAL;
	}
	if (enable && atomic_read(&g_fs15xx_switch) == 0) {
		return 0;
	}
	pr_info("%s", enable ? "Enable" : "Disable");
	atomic_set(&fs15xx->running, 0);
	hrtimer_cancel(&fs15xx->timer);
	if (enable) {
		hrtimer_start(&fs15xx->timer,
			ktime_set(FS15XX_MONITOR_PEROID / 1000,
			(FS15XX_MONITOR_PEROID % 1000) * 1000000),
			HRTIMER_MODE_REL);
		atomic_set(&fs15xx->running, 1);
	}

	return 0;
}
EXPORT_SYMBOL(fs15xx_set_timer);
#endif

int fs15xx_ext_amp_set(int enable)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	int mode;
	int ret;

	pr_debug("%s external speaker PA\n",
		enable ? "Enable" : "Disable");

	if (fs15xx == NULL) {
		return -EINVAL;
	}
	if (enable) {
		mode = atomic_read(&g_fs15xx_mode);
	} else {
		mode = FS15XX_OFF_MODE;
	}
#ifdef FSM_CHECK_PLUSE_TIME
	if (fs15xx->dev_type == FS15XX_DEV_1910) {
		ret = spc19xx_set_mode(mode);
		// fs15xx->fs15xx_mode = FS15XX_OFF_MODE; // Compare two ways
		// ret |= spc19xx_set_mode_simple(mode);
/* Add fs1512 & x102 & xx318/xx358 pa type check logic : B2 & C3 */
#ifdef FS15XX_MULTI_TYPE_CHECK
	} else if ((fs15xx->dev_type == FS15XX_DEV_X102) || (fs15xx->dev_type == FS15XX_DEV_XX318)) {
		// x102 & xx318/xx358 control function
		ret = x102_xx318_set_mode(mode);
#endif
	} else {
		ret = fs15xx_set_mode(mode);
		// fs15xx->fs15xx_mode = FS15XX_OFF_MODE; // Compare two ways
		// ret |= fs15xx_set_mode_simple(mode);
	}
#else
	if (fs15xx->dev_type == FS15XX_DEV_1910) {
		ret |= spc19xx_set_mode_simple(mode);
	} else {
		ret |= fs15xx_set_mode_simple(mode);
	}
#endif
	return ret;
}
EXPORT_SYMBOL(fs15xx_ext_amp_set);

int fs15xx_set_ext_amp(struct snd_soc_codec *codec, int enable)
{
	return fs15xx_ext_amp_set(!!enable);
}
EXPORT_SYMBOL(fs15xx_set_ext_amp);

static int fs15xx_amp_type_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;

	if (fs15xx) { // TODO, debug only
		fs15xx->dev_type = ucontrol->value.integer.value[0];
		pr_info("set dev_type:%d", fs15xx->dev_type);
	}
	return 0;
}

static int fs15xx_amp_type_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	fs15xx_dev_t *fs15xx = g_fs15xx_dev;
	ucontrol->value.integer.value[0] = ((fs15xx == NULL) ? 0 : fs15xx->dev_type);
	return 0;
}

static int fs15xx_amp_mode_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int mode = ucontrol->value.integer.value[0];

	if (mode < FS15XX_MIN_MODE || mode > FS15XX_MAX_MODE)
		return -EINVAL;

	pr_info("set mode: %d\n", mode);
	atomic_set(&g_fs15xx_mode, mode);

	return 0;
}

static int fs15xx_amp_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int mode;

	mode = atomic_read(&g_fs15xx_mode);
	if (mode < FS15XX_MIN_MODE || mode > FS15XX_MAX_MODE)
		return -EINVAL;
	ucontrol->value.integer.value[0] = mode;

	return 0;
}

static int fs15xx_amp_switch_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int enable = ucontrol->value.integer.value[0];

	fs15xx_ext_amp_set(enable);
	atomic_set(&g_fs15xx_switch, enable);

	return 0;
}

static int fs15xx_amp_switch_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int enable;

	enable = atomic_read(&g_fs15xx_switch);
	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static char const *fs15xx_type_text[] = { "Unknow", "Foursemi"};

static const struct soc_enum fs15xx_type_snd_enum = SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(fs15xx_type_text), fs15xx_type_text);

/* mode:
 * spc1910: OFF, MODE_RCV1, MODE_RCV2, MODE_SPK1, MODE_SPK2
 * fs15xx: OFF, MODE_RCV1, MODE_RCV2, MODE_RCV3, MODE_SPK1, MODE_SPK2, MODE_SPK3
 */
static char const *fs15xx_mode_text[] = {
		"OFF", "MODE_RCV1", "MODE_RCV2", "MODE_RCV3",
		"MODE_SPK1", "MODE_SPK2", "MODE_SPK3"};

static const struct soc_enum fs15xx_mode_snd_enum = SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(fs15xx_mode_text), fs15xx_mode_text);

static char const *fs15xx_switch_text[] = {"Off", "On"};
static const struct soc_enum fs15xx_switch_snd_enum = SOC_ENUM_SINGLE_EXT(
		ARRAY_SIZE(fs15xx_switch_text), fs15xx_switch_text);

static const struct snd_kcontrol_new fs15xx_amp_control[] = {
	SOC_ENUM_EXT("FSM_AMP_TYPE", fs15xx_type_snd_enum,
			fs15xx_amp_type_get, fs15xx_amp_type_set),
	SOC_ENUM_EXT("FSM_AMP_MODE", fs15xx_mode_snd_enum,
			fs15xx_amp_mode_get, fs15xx_amp_mode_set),
	SOC_ENUM_EXT("FSM_AMP_SWITCH", fs15xx_switch_snd_enum,
			fs15xx_amp_switch_get, fs15xx_amp_switch_set),
};

void fs15xx_add_card_kcontrol(struct snd_soc_card *card)
{
	snd_soc_add_card_controls(card, fs15xx_amp_control, ARRAY_SIZE(fs15xx_amp_control));
}
EXPORT_SYMBOL(fs15xx_add_card_kcontrol);

void fs15xx_add_codec_kcontrol(struct snd_soc_codec *codec)
{
	snd_soc_add_codec_controls(codec, fs15xx_amp_control, ARRAY_SIZE(fs15xx_amp_control));
}
EXPORT_SYMBOL(fs15xx_add_codec_kcontrol);

#ifdef FSM_UNUSED_CODE
void fs15xx_add_platform_kcontrol(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, fs15xx_amp_control, ARRAY_SIZE(fs15xx_amp_control));
}
EXPORT_SYMBOL(fs15xx_add_platform_kcontrol);
#endif


static int fs15xx_check_pa_type(fs15xx_dev_t *fs15xx)
{
	int level;

	if (fs15xx == NULL) {
		return -EINVAL;
	}
	if (!gpio_is_valid(fs15xx->gpio_id)) {
		return 0; // not have id pin, spc1910/fs15xx as default
	}
	gpio_direction_output(fs15xx->gpio_id, 1); // set gpio high
	udelay(1);
	level = gpio_get_value(fs15xx->gpio_id);
	gpio_set_value(fs15xx->gpio_id, 0); // set gpio low
	if (level == 0) {
		pr_info("fs15xx detected\n");
		return 0;
	}

/* Add fs1512 & x102 & xx318/xx358 pa type check logic : B2 & C3 */
#ifdef FS15XX_MULTI_TYPE_CHECK
	if (!gpio_is_valid(fs15xx->gpio_mod)) {
		pr_err("invalid gpio");
		return -EINVAL;
	}
	gpio_direction_output(fs15xx->gpio_mod, 1); // set gpio high
	udelay(1);
	level = gpio_get_value(fs15xx->gpio_mod);
	gpio_set_value(fs15xx->gpio_mod, 0); // set gpio low
	if (level == 0) {
		pr_info("x102 detected\n");
		fs15xx->dev_type = FS15XX_DEV_X102;
		return 0;
	} else if (level == 1) {
		pr_info("xx318/xx358 detected\n");
		fs15xx->dev_type = FS15XX_DEV_XX318;
		return 0;
	}
#endif

	pr_err("not fs15xx device\n");
	if (gpio_is_valid(fs15xx->gpio_id)) {
		gpio_free(fs15xx->gpio_id);
		fs15xx->gpio_id = -1;
	}
	if (gpio_is_valid(fs15xx->gpio_cmd)) {
		gpio_free(fs15xx->gpio_cmd);
		fs15xx->gpio_cmd = -1;
	}


	return -EINVAL;
}

int fs15xx_get_vbat_voltage(void)
{
	union power_supply_propval psp = { 0 };
	struct power_supply *psy;
	int vbat = 3678;

	psy = power_supply_get_by_name("battery");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
	if (psy && psy->get_property) {
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &psp);
		vbat = DIV_ROUND_CLOSEST(psp.intval, 1000);
	}
#else
	if (psy && psy->desc && psy->desc->get_property) {
		psy->desc->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &psp);
		vbat = DIV_ROUND_CLOSEST(psp.intval, 1000);
	}
#endif
	pr_info("vbat:%d", vbat);

	return vbat;
}

static int fs15xx_misc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int fs15xx_misc_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static long fs15xx_misc_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	int vbat;
	int ret;

	if (filp == NULL) {
		pr_err("invalid parameter");
		return -EINVAL;
	}

	pr_debug("cmd:%X, arg:%lX", cmd, arg);
	switch (cmd) {
	case FS15XX_IOC_GET_VBAT:
		vbat = fs15xx_get_vbat_voltage();
		ret = copy_to_user((int *)arg, &vbat, sizeof(vbat));
		if (ret) {
			pr_err("cmd:%X, copy to user fail:%d", cmd, ret);
			return -EFAULT;
		}
		break;
	default:
		pr_err("unknown cmd:%X", cmd);
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_COMPAT)
static long fs15xx_misc_compat_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	return fs15xx_misc_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static const struct file_operations g_fs15xx_misc_ops = {
	.owner   = THIS_MODULE,
	.open    = fs15xx_misc_open,
	.release = fs15xx_misc_release,
	.llseek  = no_llseek,
	.unlocked_ioctl = fs15xx_misc_ioctl,
#if defined(CONFIG_COMPAT)
	.compat_ioctl = fs15xx_misc_compat_ioctl,
#endif
};

struct miscdevice g_fs15xx_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = FS15XX_DRV_NAME,
	.fops  = &g_fs15xx_misc_ops,
	.this_device = NULL,
};

static int fs15xx_dev_probe(struct platform_device *pdev)
{
	fs15xx_dev_t *fs15xx;
	int ret = 0;

	pr_info("version: %s\n", FS15XX_DRV_VERSION);
	fs15xx = devm_kzalloc(&pdev->dev, sizeof(fs15xx_dev_t), GFP_KERNEL);
	if (fs15xx == NULL) {
		pr_err("allocate memery failed\n");
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	ret |= fs15xx_of_init(pdev, fs15xx);
#endif
#if defined(FS15XX_PINCTRL_SUPPORT)
	ret |= fs15xx_pinctrl_init(pdev, fs15xx);
#endif
	ret |= fs15xx_check_pa_type(fs15xx);
	if (ret) {
		pr_err("check pa fail:%d", ret);
		devm_kfree(&pdev->dev, fs15xx);
		return ret;
	}
	platform_set_drvdata(pdev, fs15xx);
	spin_lock_init(&fs15xx->fs15xx_lock);
	fs15xx->cur_time = ktime_get_boottime();
	ret |= fs15xx_ctrl_pin(fs15xx, FS15XX_GPIO_LOW);
	fs15xx_delay_ms(3); // 3ms+
	fs15xx->fs15xx_mode = FS15XX_OFF_MODE;
	atomic_set(&g_fs15xx_switch, 0);
	if (fs15xx->dev_type == FS15XX_DEV_15XX)
		atomic_set(&g_fs15xx_mode, FS15XX_DEFAULT_MODE);
/* Add fs1512 & x102 & xx318/xx358 pa type check logic : B2 & C3 */
#ifdef FS15XX_MULTI_TYPE_CHECK
	else if (fs15xx->dev_type == FS15XX_DEV_X102)
		atomic_set(&g_fs15xx_mode, X102_DEFAULT_MODE);
	else if (fs15xx->dev_type == FS15XX_DEV_XX318)
		atomic_set(&g_fs15xx_mode, XX318_DEFAULT_MODE);
#endif
	else
		atomic_set(&g_fs15xx_mode, SPC1910_DEFAULT_MODE);
	g_fs15xx_dev = fs15xx;

#ifdef FS15XX_USE_HRTIMER
	hrtimer_init(&fs15xx->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	fs15xx->timer.function = fs15xx_timer_callback;
	INIT_WORK(&fs15xx->monitor_work, fs15xx_monitor);
	atomic_set(&fs15xx->running, 0);
#endif
	ret = misc_register(&g_fs15xx_misc);
	if (ret) {
		pr_err("create misc failed: %d\n", ret);
	}

	pr_info("probe done!\n");

	return ret;
}

static int fs15xx_dev_remove(struct platform_device *pdev)
{
	fs15xx_dev_t *fs15xx = platform_get_drvdata(pdev);

	if (fs15xx == NULL) {
		return 0;
	}
	misc_deregister(&g_fs15xx_misc);
	if (gpio_is_valid(fs15xx->gpio_cmd)) {
		gpio_free(fs15xx->gpio_cmd);
	}
	devm_kfree(&pdev->dev, fs15xx);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fs15xx_of_match[] =
{
	{.compatible = "mediatek,spc19xx"},
	{.compatible = "foursemi,spc19xx"},
	{.compatible = "mediatek,fs15xx"},
	{.compatible = "foursemi,fs15xx"},
	{},
};
MODULE_DEVICE_TABLE(of, fs15xx_of_match);
#endif

static struct platform_driver fs15xx_dev_driver =
{
	.probe = fs15xx_dev_probe,
	.remove = fs15xx_dev_remove,
	.driver = {
		.name = FS15XX_DRV_NAME,
#ifdef CONFIG_OF
		.of_match_table = fs15xx_of_match,
#endif
	}
};

#if !defined(module_platform_driver)
static int __init fs15xx_dev_init(void)
{
	int ret;

	ret = platform_driver_register(&fs15xx_dev_driver);
	if (ret)
		pr_err("register driver failed, ret: %d\n", ret);

	return ret;
}

static void __exit fs15xx_dev_exit(void)
{
	platform_driver_unregister(&fs15xx_dev_driver);
}

module_init(fs15xx_dev_init);
module_exit(fs15xx_dev_exit);
#else
module_platform_driver(fs15xx_dev_driver);
#endif

MODULE_AUTHOR("FourSemi SW <support@foursemi.com>");
MODULE_DESCRIPTION("FourSemi Smart Power Controller driver");
MODULE_VERSION(FS15XX_DRV_VERSION);

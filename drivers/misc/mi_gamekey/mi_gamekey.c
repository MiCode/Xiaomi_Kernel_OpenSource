#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>

#undef pr_fmt
#define pr_fmt(fmt) "gamekey: " fmt

#define KEY_DOWN_LEVEL	0
#define KEY_UP_LEVEL		(1 ^ KEY_DOWN_LEVEL)
#define HALL_OUT_LEVEL	0
#define HALL_IN_LEVEL		(1 ^ HALL_OUT_LEVEL)
#define DEFAULT_DEBOUNCE_TIME	5   /*unit in ms*/

/*
#define SW_HALL_LEFT  0x03
#define SW_HALL_RIGHT 0x04
*/

#define DELAYTIME_HALL_REPORT     100   /*unit in ms*/
#define ABNORMAL_TIME_INTERVAL    95

struct delayed_work open_close_check_left;
struct delayed_work open_close_check_right;

u64 timeout_left  = 0;
u64 timeout_right = 0;

enum gpio_list {
	HALL_LEFT = 0,
	HALL_RIGHT,
	GAMEKEY_LEFT,
	GAMEKEY_RIGHT,
	GPIO_MAX,
};

struct hw_info {
	struct gpio gpio_info[GPIO_MAX]; /*hall left, right; key left, right*/
	int hall_l_irq;
	int hall_r_irq;
	int key_l_irq;
	int key_r_irq;
	bool hw_debounce;
	int debounce_time;
};

struct gamekey_core {
	struct device *dev;
	struct platform_device *pdev;
	struct miscdevice *misc;
	struct input_dev *input_dev;
	struct pinctrl *gamekey_pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct hw_info hw;
	bool   event_update;
	char   sts[GPIO_MAX];
} *g_core;

static DECLARE_WAIT_QUEUE_HEAD(event_wait);

static int detect_gpio_status_timeout(struct gamekey_core *core, int gpio_nr, int ms);

static ssize_t gamekey_read(struct file *filp, char __user *buf,
				   size_t count, loff_t *ppos)
{
	int ret = 0;
	char *sts = g_core->sts;

	if (count != GPIO_MAX)
		goto out;

	ret = copy_to_user(buf, sts, count);

	pr_debug("%s, ret=%d, Left H:%d, K:%d   Righ H:%d, K:%d",
				__func__, ret, sts[HALL_LEFT], sts[GAMEKEY_LEFT],
					sts[HALL_RIGHT], sts[GAMEKEY_RIGHT]);

	return ret ? -EFAULT : count;

out:
	return -EINVAL;
}

static ssize_t gamekey_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *ppos)
{
	pr_debug("SW_MAX = %d", SW_MAX);
	return count;
}

static unsigned int gamekey_poll(struct file *file, poll_table *wait)
{
	struct gamekey_core *core = g_core;
	unsigned long req_events = poll_requested_events(wait);
	int mask = 0;

	if (!core)
		return POLLERR;

	if (req_events & (POLLIN | POLLRDNORM)) {
		poll_wait(file, &event_wait, wait);
		if (core->event_update) {
			mask = POLLIN | POLLRDNORM;
			core->event_update = false;
		}
	}

	pr_info("poll hall event, mask = %d", mask);
	return mask;
}

static const struct file_operations misc_ops = {
	.owner		= THIS_MODULE,
	.write		= gamekey_write,
	.read		= gamekey_read,
	.poll		= gamekey_poll,
};

static struct miscdevice gamekey_misc = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "gamekey",
	.fops		= &misc_ops,
};

static int parse_dt(struct gamekey_core *core)
{
	int ret = 0;
	struct device_node *np = core->dev->of_node;

	core->hw.gpio_info[HALL_LEFT].gpio = of_get_named_gpio_flags(np, "hall_left", 0, NULL);
	core->hw.gpio_info[HALL_LEFT].flags = GPIOF_DIR_IN;
	core->hw.gpio_info[HALL_LEFT].label = "hall_left";

	core->hw.gpio_info[HALL_RIGHT].gpio = of_get_named_gpio_flags(np, "hall_right", 0, NULL);
	core->hw.gpio_info[HALL_RIGHT].flags = GPIOF_DIR_IN;
	core->hw.gpio_info[HALL_RIGHT].label = "hall_right";

	core->hw.gpio_info[GAMEKEY_LEFT].gpio = of_get_named_gpio_flags(np, "key_left", 0, NULL);
	core->hw.gpio_info[GAMEKEY_LEFT].flags = GPIOF_DIR_IN;
	core->hw.gpio_info[GAMEKEY_LEFT].label = "key_left";

	core->hw.gpio_info[GAMEKEY_RIGHT].gpio = of_get_named_gpio_flags(np, "key_right", 0, NULL);
	core->hw.gpio_info[GAMEKEY_RIGHT].flags = GPIOF_DIR_IN;
	core->hw.gpio_info[GAMEKEY_RIGHT].label = "key_right";

	ret = of_property_read_u32(np, "debounce_time", &core->hw.debounce_time);
	if (ret) {
		pr_info("do not get debounce_time from dts, use default value");
		core->hw.debounce_time = DEFAULT_DEBOUNCE_TIME;
	}


	pr_info("hall left gpio = %d",  core->hw.gpio_info[HALL_LEFT].gpio);
	pr_info("hall right gpio = %d", core->hw.gpio_info[HALL_RIGHT].gpio);
	pr_info("key left gpio = %d",  core->hw.gpio_info[GAMEKEY_LEFT].gpio);
	pr_info("key right gpio = %d",   core->hw.gpio_info[GAMEKEY_RIGHT].gpio);
	pr_info("debounce time = %d", core->hw.debounce_time);

	return 0;
}

static int input_init(struct gamekey_core *core)
{
	int ret = 0;

	core->input_dev = input_allocate_device();
	if (!core->input_dev) {
		pr_err("allocate input dev failed");
		return -ENOMEM;
	}

	core->input_dev->name = "xm_gamekey";
	core->input_dev->id.product = 0x0628;
	/*input_set_capability(core->input_dev, EV_SW,  SW_HALL_LEFT);*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F1); /*key left*/
	/*input_set_capability(core->input_dev, EV_SW,  SW_HALL_RIGHT);*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F2); /*key right*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F3); /*hall left open*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F4); /*hall left close*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F5); /*hall right open*/
	input_set_capability(core->input_dev, EV_KEY, KEY_F6); /*hall right close*/
	input_set_capability(core->input_dev, EV_SYN, SYN_REPORT);

	ret = input_register_device(core->input_dev);
	if (ret) {
		pr_err("input register failed, ret=%d", ret);
		goto input_reg_err;
	}

	pr_info("input init successful");
	return 0;

input_reg_err:
	input_free_device(core->input_dev);
	return ret;
}

#if 0
static int set_hardware_debounce(struct gamekey_core *core)
{
	struct gpio *gpio_info = core->hw.gpio_info;
	int i, ret = 0;

	for (i = 0; i < 4; i++, gpio_info++) {
		ret = gpio_set_debounce(gpio_info->gpio, core->hw.debounce_time);
		if (ret < 0)
			goto err_reset;
	}

	core->hw.hw_debounce = true;
	pr_info("support hardware debounce");
	return 0;
err_reset:
	while(i--)
		gpio_set_debounce((--gpio_info)->gpio, 0);
	return ret;
}
#endif

/* This function may sleep
 * @return: negative indicate situation unstable
 * */
static int detect_gpio_status(struct gamekey_core *core, int gpio_nr)
{
	int sts, ret = -1;
	bool stable = false;

	sts = gpio_get_value(core->hw.gpio_info[gpio_nr].gpio);
	if (!core->hw.hw_debounce) {
		msleep(core->hw.debounce_time);
		if (sts == gpio_get_value(core->hw.gpio_info[gpio_nr].gpio))
			stable = true;
	} else {
		stable = true;
	}

	if (!stable) {
		pr_err("gpio_list[%d] unstable, can't get status", gpio_nr);
		return -1;
	// } else {
	// 	pr_err("gpio_list[%d] running..., sts=%d", gpio_nr, sts);
	}

	switch(gpio_nr) {
		case HALL_LEFT:
		case HALL_RIGHT:
			ret = (sts == HALL_OUT_LEVEL ? 1 : 0);
			// pr_err("HALL is running...,ret = %d", ret);
		break;
		case GAMEKEY_LEFT:
		case GAMEKEY_RIGHT:
			ret = (sts == KEY_DOWN_LEVEL? 1 : 0);
			// pr_err("KEY is running...,ret = %d", ret);
		break;
	};

	return ret;
}

static int detect_gpio_status_timeout(struct gamekey_core *core,
		int gpio_nr, int ms)
{
	int sts = -1;
	u64 timeout = get_jiffies_64() + msecs_to_jiffies(ms);

	while (time_is_after_jiffies64(timeout)) {
		sts = detect_gpio_status(core, gpio_nr);
		if (sts < 0)
			msleep(50);
		else
			break;
	}

	return sts;
}

static irqreturn_t hall_left_handler(int irq, void *data)
{
	struct gamekey_core *core = data;
	struct input_dev *input_dev = core->input_dev;

	core->sts[HALL_LEFT] = detect_gpio_status(core, HALL_LEFT);
	if (core->sts[HALL_LEFT] >= 0) {
		/*
		input_report_switch(input_dev, SW_HALL_LEFT, core->sts[HALL_LEFT]);
		input_sync(core->input_dev);
		*/
		if (core->sts[HALL_LEFT] == 1) {
			timeout_left = get_jiffies_64();
			schedule_delayed_work(&open_close_check_left, msecs_to_jiffies(DELAYTIME_HALL_REPORT));
			pr_info("Enter-open state of left hall");

		} else {
			timeout_left = get_jiffies_64()- timeout_left;
			if (jiffies_to_msecs(timeout_left)<ABNORMAL_TIME_INTERVAL) {
				pr_info(" dont report Enter-open state of left hall if less than 100ms");
				cancel_delayed_work_sync(&open_close_check_left);
			} else {
				input_event(input_dev, EV_KEY, KEY_F4, 1);
				input_sync(core->input_dev);
				input_event(input_dev, EV_KEY, KEY_F4, 0);
				input_sync(core->input_dev);
				pr_info("%s-hall left close",__func__);
			}
		}

		core->event_update = true;
		wake_up_interruptible(&event_wait);
	}

	pr_info("%s, sts = %d", __func__, core->sts[HALL_LEFT]);
	return IRQ_HANDLED;
}

static irqreturn_t hall_right_handler(int irq, void *data)
{
	struct gamekey_core *core = data;
	struct input_dev *input_dev = core->input_dev;

	core->sts[HALL_RIGHT] = detect_gpio_status(core, HALL_RIGHT);
	if (core->sts[HALL_RIGHT] >= 0) {
	/*
		input_report_switch(input_dev, SW_HALL_RIGHT, core->sts[HALL_RIGHT]);
		input_sync(core->input_dev);
	*/
		if (core->sts[HALL_RIGHT] == 1) {
			timeout_right = get_jiffies_64();
			schedule_delayed_work(&open_close_check_right, msecs_to_jiffies(DELAYTIME_HALL_REPORT));
			pr_info("Enter-open state of right hall");
		} else {
			timeout_right = get_jiffies_64()- timeout_right;
			if (jiffies_to_msecs(timeout_right)<ABNORMAL_TIME_INTERVAL) {
				pr_info(" dont report Enter-open state of right hall if less than 100ms");
				cancel_delayed_work_sync(&open_close_check_right);
			} else {
				input_event(input_dev, EV_KEY, KEY_F6, 1);
				input_sync(core->input_dev);
				input_event(input_dev, EV_KEY, KEY_F6, 0);
				input_sync(core->input_dev);
				pr_info("%s-hall right close",__func__);
			}
        }

		core->event_update = true;
		wake_up_interruptible(&event_wait);
	}

	pr_info("%s, sts = %d", __func__, core->sts[HALL_RIGHT]);
	return IRQ_HANDLED;
}

static irqreturn_t key_left_handler(int irq, void *data)
{
	struct gamekey_core *core = data;
	struct input_dev *input_dev = core->input_dev;

	core->sts[GAMEKEY_LEFT] = detect_gpio_status(core, GAMEKEY_LEFT);
	if (core->sts[GAMEKEY_LEFT] >= 0) {
		input_event(input_dev, EV_KEY, KEY_F1, core->sts[GAMEKEY_LEFT]);
		input_sync(core->input_dev);
		core->event_update = true;
		wake_up_interruptible(&event_wait);
	}

	pr_info("%s, sts = %d", __func__, core->sts[GAMEKEY_LEFT]);
	return IRQ_HANDLED;
}

static irqreturn_t key_right_handler(int irq, void *data)
{
	struct gamekey_core *core = data;
	struct input_dev *input_dev = core->input_dev;

	core->sts[GAMEKEY_RIGHT] = detect_gpio_status(core, GAMEKEY_RIGHT);
	if (core->sts[GAMEKEY_RIGHT] >= 0) {
		input_event(input_dev, EV_KEY, KEY_F2, core->sts[GAMEKEY_RIGHT]);
		input_sync(core->input_dev);
		core->event_update = true;
		wake_up_interruptible(&event_wait);
	}

	pr_info("%s, sts = %d", __func__, core->sts[GAMEKEY_RIGHT]);
	return IRQ_HANDLED;
}

static int pinctrl_init(struct gamekey_core *core)
{
	int ret = 0;

	core->gamekey_pinctrl = devm_pinctrl_get(core->dev);
	if (IS_ERR_OR_NULL(core->gamekey_pinctrl)) {
		ret = PTR_ERR(core->gamekey_pinctrl);
		goto pinctrl_err;
	}

	core->pinctrl_state_active
		= pinctrl_lookup_state(core->gamekey_pinctrl, "gamekey_active");
	if (IS_ERR_OR_NULL(core->pinctrl_state_active)) {
		ret = PTR_ERR(core->pinctrl_state_active);
		goto pinctrl_err;
	}

	core->pinctrl_state_suspend
		= pinctrl_lookup_state(core->gamekey_pinctrl, "gamekey_suspend");
	if (IS_ERR_OR_NULL(core->pinctrl_state_suspend)) {
		ret = PTR_ERR(core->pinctrl_state_suspend);
		goto pinctrl_err;
	}

	pr_info("pinctrl init successful");
	return 0;

pinctrl_err:
	devm_pinctrl_put(core->gamekey_pinctrl);
	core->gamekey_pinctrl = NULL;
	return ret;
}

static int gpio_init(struct gamekey_core *core)
{
	int ret = 0;
	struct hw_info *hw = &core->hw;

	ret = gpio_request_array(hw->gpio_info, GPIO_MAX);
	if (ret) {
		pr_err("request gpios failed, ret = %d", ret);
		goto gpio_err;
	}

	/*
	set_hardware_debounce(core);
	pr_info("hardware debounce: %d", hw->hw_debounce);
	*/
	hw->hw_debounce = false;

	ret = pinctrl_init(core);
	if (ret) {
		pr_err("pinctrl init failed, ret = %d", ret);
		goto pinctrl_init_err;
	}

	ret = pinctrl_select_state(core->gamekey_pinctrl, core->pinctrl_state_active);
	if (ret) {
		pr_err("pinctrl select failed, ret = %d", ret);
		goto pinctrl_select_err;
	}

	pr_info("gpio init successful");
	return 0;

pinctrl_select_err:
	devm_pinctrl_put(core->gamekey_pinctrl);
pinctrl_init_err:
	gpio_free_array(hw->gpio_info, GPIO_MAX);
gpio_err:
	return ret;
}

static int irq_init(struct gamekey_core *core)
{
	int ret = -1;
	struct hw_info *hw = &core->hw;

	hw->hall_l_irq = gpio_to_irq(hw->gpio_info[0].gpio);
	hw->hall_r_irq = gpio_to_irq(hw->gpio_info[1].gpio);
	hw->key_l_irq =  gpio_to_irq(hw->gpio_info[2].gpio);
	hw->key_r_irq =  gpio_to_irq(hw->gpio_info[3].gpio);

	if ((hw->hall_l_irq > 0) && (hw->hall_r_irq > 0) && \
			(hw->key_l_irq > 0)  && (hw->key_r_irq > 0)) {

		ret = request_threaded_irq(hw->hall_l_irq, NULL, hall_left_handler,
				IRQF_TRIGGER_RISING| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hall_l_irq", core);
		if (ret) {
			pr_err("request irq handler for hall left failed, ret = %d", ret);
			goto hl_err;
		}
		enable_irq_wake(hw->hall_l_irq);

		ret = request_threaded_irq(hw->hall_r_irq, NULL, hall_right_handler,
				IRQF_TRIGGER_RISING| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"hall_r_irq", core);
		if (ret) {
			pr_err("request irq handler for hall right failed, ret = %d", ret);
			goto hr_err;
		}
		enable_irq_wake(hw->hall_r_irq);

		ret = request_threaded_irq(hw->key_l_irq, NULL, key_left_handler,
				IRQF_TRIGGER_RISING| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"key_l_irq", core);
		if (ret) {
			pr_err("request irq handler for key left failed, ret = %d", ret);
			goto kl_err;
		}
		enable_irq_wake(hw->key_l_irq);

		ret = request_threaded_irq(hw->key_r_irq, NULL, key_right_handler,
				IRQF_TRIGGER_RISING| IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				"key_r_irq", core);
		if (ret) {
			pr_err("request irq handler for key right failed, ret = %d", ret);
			goto kr_err;
		}
		enable_irq_wake(hw->key_r_irq);
	} else {
		pr_err("irq number illegal, hall left: %d,\
				hall right: %d, key left: %d, key right: %d", \
				hw->hall_l_irq, hw->hall_r_irq, hw->key_l_irq, hw->key_r_irq);
		goto irqno_err;
	}

	pr_info("irq init successful");
	return 0;

kr_err:
	free_irq(hw->key_l_irq, core);
kl_err:
	free_irq(hw->hall_r_irq, core);
hr_err:
	free_irq(hw->hall_l_irq, core);
hl_err:
irqno_err:
	return ret;
}

static int power_init(struct gamekey_core *core)
{
	int ret = -1;
	struct regulator *vreg;
	struct device *dev = core->dev;

	pr_info("Try to enable gamekey_vreg\n");
	vreg = regulator_get(dev, "gamekey_vreg");
	if (vreg == NULL) {
		pr_err("gamekey_vreg regulator get failed!\n");
		goto rg_err;
	}

	if (regulator_is_enabled(vreg)) {
		pr_info("gamekey_vreg is already enabled!\n");
	} else {
		ret = regulator_enable(vreg);
		if (ret) {
			pr_err("error enabling gamekey_vreg!\n");
			vreg = NULL;
			goto rg_err;
		}
	}

	ret = regulator_get_voltage(vreg);
	pr_info("%s regulator_value %d!\n", __func__,ret);

	pr_info("power init successful");
	return 0;

rg_err:
	return -1;
}

static int get_status_after_boot(struct gamekey_core *core)
{
	int i;
	struct input_dev *input_dev = core->input_dev;

	for (i = 0; i < GPIO_MAX; i++)
		core->sts[i] = (char)detect_gpio_status_timeout(core, i, 500);

	/*
	input_report_switch(input_dev, SW_HALL_LEFT, core->sts[HALL_LEFT]);
	input_report_switch(input_dev, SW_HALL_RIGHT, core->sts[HALL_RIGHT]);
	*/
	if (core->sts[HALL_LEFT] == 1) {
		input_event(input_dev, EV_KEY, KEY_F3, 1);
		input_sync(core->input_dev);
		input_event(input_dev, EV_KEY, KEY_F3, 0);
		input_sync(core->input_dev);
		pr_info("%s-hall left open",__func__);
	} else {
		input_event(input_dev, EV_KEY, KEY_F4, 1);
		input_sync(core->input_dev);
		input_event(input_dev, EV_KEY, KEY_F4, 0);
		input_sync(core->input_dev);
		pr_info("%s-hall left close",__func__);
	}

	if (core->sts[HALL_RIGHT] == 1) {
		input_event(input_dev, EV_KEY, KEY_F5, 1);
		input_sync(core->input_dev);
		input_event(input_dev, EV_KEY, KEY_F5, 0);
		input_sync(core->input_dev);
		pr_info("%s-hall right open",__func__);
	} else {
		input_event(input_dev, EV_KEY, KEY_F6, 1);
		input_sync(core->input_dev);
		input_event(input_dev, EV_KEY, KEY_F6, 0);
		input_sync(core->input_dev);
		pr_info("%s-hall right close",__func__);
	}

	input_event(input_dev, EV_KEY, KEY_F1, core->sts[GAMEKEY_LEFT]);
	input_event(input_dev, EV_KEY, KEY_F2, core->sts[GAMEKEY_RIGHT]);
	input_sync(core->input_dev);

	pr_info("%s, Left H:%d, K:%d   Righ H:%d, K:%d",
				__func__, core->sts[HALL_LEFT], core->sts[GAMEKEY_LEFT],
					core->sts[HALL_RIGHT], core->sts[GAMEKEY_RIGHT]);

	return 0;
}


static void game_open_close_check_left(struct work_struct *work)
{
	struct input_dev *input_dev = g_core->input_dev;

	input_event(input_dev, EV_KEY, KEY_F3, 1);
	input_sync(input_dev);
	input_event(input_dev, EV_KEY, KEY_F3, 0);
	input_sync(input_dev);
	pr_info("%s-hall left open",__func__);
}

static void game_open_close_check_right(struct work_struct *work)
{
	struct input_dev *input_dev = g_core->input_dev;

	input_event(input_dev, EV_KEY, KEY_F5, 1);
	input_sync(input_dev);
	input_event(input_dev, EV_KEY, KEY_F5, 0);
	input_sync(input_dev);
	pr_info("%s-hall right open",__func__);
}

static int __init gamekey_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct gamekey_core *core;

	core = kzalloc(sizeof(*core), GFP_KERNEL);
	if (!core) {
		pr_err("allocated mem for gamekey failed, exiting probe...");
		return -ENOMEM;
	}

	g_core = core;
	core->pdev = pdev;
	core->dev = &pdev->dev;
	platform_set_drvdata(pdev, core);

	INIT_DELAYED_WORK(&open_close_check_left, game_open_close_check_left);
	INIT_DELAYED_WORK(&open_close_check_right, game_open_close_check_right);

	ret = parse_dt(core);

	ret = input_init(core);
	if (ret) {
		pr_err("input init failed, exiting probe...");
		goto input_err;
	}

	ret = gpio_init(core);
	if (ret) {
		pr_err("gpio init failed, exiting probe...");
		goto gpio_err;
	}

	ret = irq_init(core);
	if (ret) {
		pr_err("irq init failed, exiting probe...");
		goto irq_err;
	}

	ret = power_init(core);
	if (ret) {
		pr_err("power_init failed, may cause gamekey function abnormally!");
	}

	get_status_after_boot(core);

	gamekey_misc.parent = &pdev->dev;
	core->misc = &gamekey_misc;
	ret = misc_register(&gamekey_misc);
	if (ret) {
		pr_err("misc regist failed, ret = %d, exiting probe...", ret);
		goto misc_err;
	}

	pr_info("probe finished!");

	return 0;

misc_err:
	free_irq(core->hw.key_l_irq, core);
	free_irq(core->hw.key_r_irq, core);
	free_irq(core->hw.hall_r_irq, core);
	free_irq(core->hw.hall_l_irq, core);
irq_err:
	devm_pinctrl_put(core->gamekey_pinctrl);
	gpio_free_array(core->hw.gpio_info, GPIO_MAX);
gpio_err:
	input_unregister_device(core->input_dev);
	input_free_device(core->input_dev);
input_err:
	pr_err("ERROR: gamekey probe failed");
	return ret;
}

static int __exit gamekey_remove(struct platform_device *pdev)
{
	struct gamekey_core *core = platform_get_drvdata(pdev);
	misc_deregister(core->misc);
	free_irq(core->hw.key_l_irq, core);
	free_irq(core->hw.key_r_irq, core);
	free_irq(core->hw.hall_r_irq, core);
	free_irq(core->hw.hall_l_irq, core);
	devm_pinctrl_put(core->gamekey_pinctrl);
	gpio_free_array(core->hw.gpio_info, GPIO_MAX);
	input_unregister_device(core->input_dev);
	input_free_device(core->input_dev);

	pr_info("gamekey remove done");
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "xm,gamekey" },
	{}
};

static struct platform_driver gamekey_pdrv = {
	.driver = {
		.name = "gamekey",
		.of_match_table = dt_match,
	},
	.remove = __exit_p(gamekey_remove),
};

module_platform_driver_probe(gamekey_pdrv, gamekey_probe);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xiaomi, jianghao");
MODULE_DESCRIPTION("game key driver");

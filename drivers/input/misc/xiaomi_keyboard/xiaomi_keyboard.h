#ifndef __XIAOMI_KEYBOARD_H
#define __XIAOMI_KEYBOARD_H

#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#define XIAOMI_KB_TAG "xiaomi-keyboard"
#define MI_KB_INFO(fmt, args...)    pr_info("[%s] %s %d: " fmt, XIAOMI_KB_TAG, __func__, __LINE__, ##args)
#define MI_KB_ERR(fmt, args...)    pr_err("[%s] %s %d: " fmt, XIAOMI_KB_TAG, __func__, __LINE__, ##args)

struct xiaomi_keyboard_platdata {
	u32 rst_gpio;
	u32 rst_flags;
	u32 in_irq_gpio;
	u32 in_irq_flags;
	u32 vdd_gpio;
};

struct xiaomi_keyboard_data {
	struct notifier_block drm_notif;
	struct xiaomi_keyboard_platdata *pdata;
	bool dev_pm_suspend;
	int irq;
	struct platform_device *pdev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_active;
	struct pinctrl_state *pins_suspend;
	struct workqueue_struct *event_wq;
	struct work_struct resume_work;
	struct work_struct suspend_work;
	int keyboard_conn_status;
	struct mutex rw_mutex;

	struct mutex power_supply_lock;
	struct work_struct power_supply_work;
	struct notifier_block power_supply_notifier;
	int is_usb_exist;
	bool keyboard_is_enable;
	bool is_in_suspend;
};
#endif

/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "kpd.h"
#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/pinctrl/consumer.h>

#define KPD_NAME	"mtk-kpd"

#ifdef CONFIG_LONG_PRESS_MODE_EN
struct timer_list Long_press_key_timer;
atomic_t vol_down_long_press_flag = ATOMIC_INIT(0);
#endif

int kpd_klog_en;
void __iomem *kp_base;
static unsigned int kp_irqnr;
struct input_dev *kpd_input_dev;
static struct dentry *kpd_droot;
static struct dentry *kpd_dklog;
unsigned long call_status;
static bool kpd_suspend;
static unsigned int kp_irqnr;
static u32 kpd_keymap[KPD_NUM_KEYS];
static u16 kpd_keymap_state[KPD_NUM_MEMS];

struct input_dev *kpd_input_dev;
#ifdef CONFIG_PM_SLEEP
struct wakeup_source* kpd_suspend_lock;
#endif
struct keypad_dts_data kpd_dts_data;

/* for keymap handling */
static void kpd_keymap_handler(unsigned long data);
static DECLARE_TASKLET(kpd_keymap_tasklet, kpd_keymap_handler, 0);

static void kpd_memory_setting(void);
static int kpd_pdrv_probe(struct platform_device *pdev);
static int kpd_pdrv_suspend(struct platform_device *pdev, pm_message_t state);
static int kpd_pdrv_resume(struct platform_device *pdev);
static struct platform_driver kpd_pdrv;

static void kpd_memory_setting(void)
{
	kpd_init_keymap(kpd_keymap);
	kpd_init_keymap_state(kpd_keymap_state);
}

#if 1
static ssize_t kpd_call_state_store(struct device_driver *ddri,
		const char *buf, size_t count)
{
	int ret;

	ret = kstrtoul(buf, 10, &call_status);
	if (ret) {
		kpd_print("kpd call state: Invalid values\n");
		return -EINVAL;
	}

	switch (call_status) {
	case 1:
		kpd_print("kpd call state: Idle state!\n");
		break;
	case 2:
		kpd_print("kpd call state: ringing state!\n");
		break;
	case 3:
		kpd_print("kpd call state: active or hold state!\n");
		break;

	default:
		kpd_print("kpd call state: Invalid values\n");
		break;
	}
	return count;
}

static ssize_t kpd_call_state_show(struct device_driver *ddri, char *buf)
{
	ssize_t res;

	res = snprintf(buf, PAGE_SIZE, "%ld\n", call_status);
	return res;
}

static DRIVER_ATTR_RW(kpd_call_state);
static struct driver_attribute *kpd_attr_list[] = {
	&driver_attr_kpd_call_state,
};


static int kpd_create_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(kpd_attr_list);

	if (driver == NULL)
		return -EINVAL;

	for (idx = 0; idx < num; idx++) {
		err = driver_create_file(driver, kpd_attr_list[idx]);
		if (err) {
			kpd_info("driver_create_file (%s) = %d\n",
				kpd_attr_list[idx]->attr.name, err);
			break;
		}
	}
	return err;
}

static int kpd_delete_attr(struct device_driver *driver)
{
	int idx, err = 0;
	int num = ARRAY_SIZE(kpd_attr_list);

	if (!driver)
		return -EINVAL;

	for (idx = 0; idx < num; idx++)
		driver_remove_file(driver, kpd_attr_list[idx]);

	return err;
}
#endif
/****************************************/
#ifdef CONFIG_LONG_PRESS_MODE_EN
void vol_down_long_press(unsigned long pressed)
{
	atomic_set(&vol_down_long_press_flag, 1);
}
#endif
/*****************************************/

#ifdef CONFIG_KPD_PWRKEY_USE_PMIC
void kpd_pwrkey_pmic_handler(unsigned long pressed)
{
	kpd_print("Power Key generate, pressed=%ld\n", pressed);
	if (!kpd_input_dev) {
		kpd_print("KPD input device not ready\n");
		return;
	}
	kpd_pmic_pwrkey_hal(pressed);
}
#endif

void kpd_pmic_rstkey_handler(unsigned long pressed)
{
	kpd_print("PMIC reset Key generate, pressed=%ld\n", pressed);
	if (!kpd_input_dev) {
		kpd_print("KPD input device not ready\n");
		return;
	}
	kpd_pmic_rstkey_hal(pressed);
}

static void kpd_keymap_handler(unsigned long data)
{
	u16 i, j;
	int32_t pressed;
	u16 new_state[KPD_NUM_MEMS], change, mask;
	u16 hw_keycode, linux_keycode;

	kpd_get_keymap_state(new_state);
#ifdef CONFIG_PM_SLEEP
	__pm_wakeup_event(kpd_suspend_lock, 500);
#endif
	for (i = 0; i < KPD_NUM_MEMS; i++) {
		change = new_state[i] ^ kpd_keymap_state[i];
		if (change == 0U)
			continue;

		for (j = 0; j < 16U; j++) {
			mask = (u16) 1 << j;
			if ((change & mask) == 0U)
				continue;

			hw_keycode = (i << 4) + j;

			if (hw_keycode >= KPD_NUM_KEYS)
				continue;

			/* bit is 1: not pressed, 0: pressed */
			pressed = ((new_state[i] & mask) == 0U) ? 1 : 0;
			kpd_print("(%s) HW keycode = %d\n",
				(pressed == 1) ? "pressed" : "released",
					hw_keycode);

			linux_keycode = kpd_keymap[hw_keycode];
			if (linux_keycode == 0U)
				continue;
			input_report_key(kpd_input_dev, linux_keycode, pressed);
			input_sync(kpd_input_dev);
			kpd_print("report Linux keycode = %d\n", linux_keycode);

#ifdef CONFIG_LONG_PRESS_MODE_EN
			if (pressed) {
				init_timer(&Long_press_key_timer);
				Long_press_key_timer.expires = jiffies + 5*HZ;
				Long_press_key_timer.data =
					(unsigned long)pressed;
				Long_press_key_timer.function =
					vol_down_long_press;
				add_timer(&Long_press_key_timer);
			} else {
				del_timer_sync(&Long_press_key_timer);
			}
			if (!pressed &&
				atomic_read(&vol_down_long_press_flag)) {
				atomic_set(&vol_down_long_press_flag, 0);
			}
#endif
		}
	}

	memcpy(kpd_keymap_state, new_state, sizeof(new_state));
	enable_irq(kp_irqnr);
}

static irqreturn_t kpd_irq_handler(int irq, void *dev_id)
{
	/* use _nosync to avoid deadlock */
	disable_irq_nosync(kp_irqnr);
	tasklet_schedule(&kpd_keymap_tasklet);
	return IRQ_HANDLED;
}

static int kpd_open(struct input_dev *dev)
{
	/* void __user *uarg = (void __user *)arg; */
	return 0;
}

void kpd_get_dts_info(struct device_node *node)
{
	int32_t ret;

	of_property_read_u32(node, "mediatek,kpd-key-debounce",
		&kpd_dts_data.kpd_key_debounce);
	of_property_read_u32(node, "mediatek,kpd-sw-pwrkey",
		&kpd_dts_data.kpd_sw_pwrkey);
	of_property_read_u32(node, "mediatek,kpd-hw-pwrkey",
		&kpd_dts_data.kpd_hw_pwrkey);
	of_property_read_u32(node, "mediatek,kpd-sw-rstkey",
		&kpd_dts_data.kpd_sw_rstkey);
	of_property_read_u32(node, "mediatek,kpd-hw-rstkey",
		&kpd_dts_data.kpd_hw_rstkey);
	of_property_read_u32(node, "mediatek,kpd-use-extend-type",
		&kpd_dts_data.kpd_use_extend_type);
	of_property_read_u32(node, "mediatek,kpd-hw-dl-key1",
		&kpd_dts_data.kpd_hw_dl_key1);
	of_property_read_u32(node, "mediatek,kpd-hw-dl-key2",
		&kpd_dts_data.kpd_hw_dl_key2);
	of_property_read_u32(node, "mediatek,kpd-hw-dl-key3",
		&kpd_dts_data.kpd_hw_dl_key3);
	of_property_read_u32(node, "mediatek,kpd-hw-recovery-key",
		&kpd_dts_data.kpd_hw_recovery_key);
	of_property_read_u32(node, "mediatek,kpd-hw-factory-key",
		&kpd_dts_data.kpd_hw_factory_key);
	of_property_read_u32(node, "mediatek,kpd-hw-map-num",
		&kpd_dts_data.kpd_hw_map_num);
	of_property_read_u32(node, "mediatek,boot_mode",
		&kpd_dts_data.boot_mode);
	ret = of_property_read_u32_array(node, "mediatek,kpd-hw-init-map",
		kpd_dts_data.kpd_hw_init_map,
			kpd_dts_data.kpd_hw_map_num);

	if (ret) {
		kpd_print("kpd-hw-init-map was not defined in dts.\n");
		memset(kpd_dts_data.kpd_hw_init_map, 0,
			sizeof(kpd_dts_data.kpd_hw_init_map));
	}

	kpd_print("deb= %d, sw-pwr= %d, hw-pwr= %d, hw-rst= %d, sw-rst= %d\n",
		  kpd_dts_data.kpd_key_debounce, kpd_dts_data.kpd_sw_pwrkey,
			kpd_dts_data.kpd_hw_pwrkey, kpd_dts_data.kpd_hw_rstkey,
				kpd_dts_data.kpd_sw_rstkey);
}

static int32_t kpd_gpio_init(struct device *dev)
{
	struct pinctrl *keypad_pinctrl;
	struct pinctrl_state *kpd_default;
	int32_t ret;

	if (dev == NULL) {
		kpd_print("kpd device is NULL!\n");
		ret = -1;
	} else {
		keypad_pinctrl = devm_pinctrl_get(dev);
		if (IS_ERR(keypad_pinctrl)) {
			ret = -1;
			kpd_print("Cannot find keypad_pinctrl!\n");
		} else {
			kpd_default = pinctrl_lookup_state(keypad_pinctrl,
				"default");
			if (IS_ERR(kpd_default)) {
				ret = -1;
				kpd_print("Cannot find ecall_state!\n");
			} else
				ret = pinctrl_select_state(keypad_pinctrl,
					kpd_default);
		}
	}
	return ret;
}

static int mt_kpd_debugfs(void)
{
#ifdef CONFIG_MTK_ENG_BUILD
	kpd_klog_en = 1;
#else
	kpd_klog_en = 0;
#endif
	kpd_droot = debugfs_create_dir("keypad", NULL);
	if (IS_ERR_OR_NULL(kpd_droot))
		return PTR_ERR(kpd_droot);

	kpd_dklog = debugfs_create_u32("debug", 0600, kpd_droot, &kpd_klog_en);

	return 0;
}

static int kpd_pdrv_probe(struct platform_device *pdev)
{
	struct clk *kpd_clk = NULL;
	u32 i;
	int32_t err = 0;
	printk("kpd_pdrv_probe\n");
	if (!pdev->dev.of_node) {
		kpd_notice("no kpd dev node\n");
		return -ENODEV;
	}

	kpd_clk = devm_clk_get(&pdev->dev, "kpd-clk");
	if (!IS_ERR(kpd_clk)) {
		err = clk_prepare_enable(kpd_clk);
		if (err)
			kpd_notice("get kpd-clk fail: %d\n", err);
	} else {
		kpd_notice("kpd-clk is default set by ccf.\n");
	}

	kp_base = of_iomap(pdev->dev.of_node, 0);
	if (!kp_base) {
		kpd_notice("KP iomap failed\n");
		return -ENODEV;
	};

	kp_irqnr = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!kp_irqnr) {
		kpd_notice("KP get irqnr failed\n");
		return -ENODEV;
	}
	kpd_info("kp base: 0x%p, addr:0x%p,  kp irq: %d\n",
			kp_base, &kp_base, kp_irqnr);
	err = kpd_gpio_init(&pdev->dev);
	if (err != 0)
		kpd_print("gpio init failed\n");

	kpd_get_dts_info(pdev->dev.of_node);

	kpd_memory_setting();

	kpd_input_dev = devm_input_allocate_device(&pdev->dev);
	if (!kpd_input_dev) {
		kpd_notice("input allocate device fail.\n");
		return -ENOMEM;
	}

	kpd_input_dev->name = KPD_NAME;
	kpd_input_dev->id.bustype = BUS_HOST;
	kpd_input_dev->id.vendor = 0x2454;
	kpd_input_dev->id.product = 0x6500;
	kpd_input_dev->id.version = 0x0010;
	kpd_input_dev->open = kpd_open;
	kpd_input_dev->dev.parent = &pdev->dev;

	__set_bit(EV_KEY, kpd_input_dev->evbit);
#if defined(CONFIG_KPD_PWRKEY_USE_PMIC)
	__set_bit(kpd_dts_data.kpd_sw_pwrkey, kpd_input_dev->keybit);
	kpd_keymap[8] = 0;
#endif
	if (!kpd_dts_data.kpd_use_extend_type) {
		for (i = 17; i < KPD_NUM_KEYS; i += 9)
			kpd_keymap[i] = 0;
	}
	for (i = 0; i < KPD_NUM_KEYS; i++) {
		if (kpd_keymap[i] != 0)
			__set_bit(kpd_keymap[i], kpd_input_dev->keybit);
	}

	if (kpd_dts_data.kpd_sw_rstkey)
		__set_bit(kpd_dts_data.kpd_sw_rstkey, kpd_input_dev->keybit);
#ifdef KPD_KEY_MAP
	__set_bit(KPD_KEY_MAP, kpd_input_dev->keybit);
#endif
#ifdef CONFIG_MTK_MRDUMP_KEY
	__set_bit(KEY_RESTART, kpd_input_dev->keybit);
#endif

	err = input_register_device(kpd_input_dev);
	if (err) {
		kpd_notice("register input device failed (%d)\n", err);
		return err;
	}
#ifdef CONFIG_PM_SLEEP
	kpd_suspend_lock = wakeup_source_register(NULL, "kpd wakelock");
#endif
	/* register IRQ and EINT */
	kpd_set_debounce(kpd_dts_data.kpd_key_debounce);
	err = request_irq(kp_irqnr, kpd_irq_handler, IRQF_TRIGGER_NONE,
			KPD_NAME, NULL);
	if (err) {
		kpd_notice("register IRQ failed (%d)\n", err);
		input_unregister_device(kpd_input_dev);
		return err;
	}

	if (enable_irq_wake(kp_irqnr) < 0)
		kpd_notice("irq %d enable irq wake fail\n", kp_irqnr);

#ifdef CONFIG_MTK_MRDUMP_KEY
	mt_eint_register();
#endif
#ifdef CONFIG_MTK_PMIC_NEW_ARCH
	long_press_reboot_function_setting();
#endif
	err = kpd_create_attr(&kpd_pdrv.driver);
	if (err) {
		kpd_notice("create attr file fail\n");
		kpd_delete_attr(&kpd_pdrv.driver);
		return err;
	}
	/* Add kpd debug node */
	mt_kpd_debugfs();

	kpd_info("kpd_probe OK.\n");

	return err;
}

static int kpd_pdrv_suspend(struct platform_device *pdev, pm_message_t state)
{
	kpd_suspend = true;
#ifdef MTK_KP_WAKESOURCE
	if (call_status == 2) {
		kpd_print("kpd_early_suspend wake up source enable!! (%d)\n",
				kpd_suspend);
	} else {
		kpd_wakeup_src_setting(0);
		kpd_print("kpd_early_suspend wake up source disable!! (%d)\n",
				kpd_suspend);
	}
#endif
	kpd_print("suspend!! (%d)\n", kpd_suspend);
	return 0;
}

static int kpd_pdrv_resume(struct platform_device *pdev)
{
	kpd_suspend = false;
#ifdef MTK_KP_WAKESOURCE
	if (call_status == 2) {
		kpd_print("kpd_early_suspend wake up source enable!! (%d)\n",
				kpd_suspend);
	} else {
		kpd_print("kpd_early_suspend wake up source resume!! (%d)\n",
				kpd_suspend);
		kpd_wakeup_src_setting(1);
	}
#endif
	kpd_print("resume!! (%d)\n", kpd_suspend);
	return 0;
}

static const struct of_device_id kpd_of_match[] = {
	{.compatible = "mediatek,mt8167-keypad"},
	{.compatible = "mediatek,kp"},
	{},
};

static struct platform_driver kpd_pdrv = {
	.probe = kpd_pdrv_probe,
	.suspend = kpd_pdrv_suspend,
	.resume = kpd_pdrv_resume,
	.driver = {
		   .name = KPD_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = kpd_of_match,
		   },
};

module_platform_driver(kpd_pdrv);

MODULE_AUTHOR("Mediatek Corporation");
MODULE_DESCRIPTION("MTK Keypad (KPD) Driver");
MODULE_LICENSE("GPL");

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "ilitek_v3.h"

#define DTS_INT_GPIO	"touch,irq-gpio"
#define DTS_RESET_GPIO	"touch,reset-gpio"
#define DTS_OF_NAME	"ilitek"
#define MTK_RST_GPIO	GTP_RST_PORT
#define MTK_INT_GPIO	GTP_INT_PORT

extern struct tpd_device *tpd;

void ili_tp_reset(void)
{
	ILI_INFO("edge delay = %d\n", ilits->rst_edge_delay);

	/* Need accurate power sequence, do not change it to msleep */
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(1);
	tpd_gpio_output(ilits->tp_rst, 0);
	mdelay(5);
	tpd_gpio_output(ilits->tp_rst, 1);
	mdelay(ilits->rst_edge_delay);
}

void ili_input_register(void)
{
	int i;

	ilits->input = tpd->dev;

	if (tpd_dts_data.use_tpd_button) {
		for (i = 0; i < tpd_dts_data.tpd_key_num; i++)
			input_set_capability(ilits->input, EV_KEY, tpd_dts_data.tpd_key_local[i]);
	}

	/* set the supported event type for input device */
	set_bit(EV_ABS, ilits->input->evbit);
	set_bit(EV_SYN, ilits->input->evbit);
	set_bit(EV_KEY, ilits->input->evbit);
	set_bit(BTN_TOUCH, ilits->input->keybit);
	set_bit(BTN_TOOL_FINGER, ilits->input->keybit);
	set_bit(INPUT_PROP_DIRECT, ilits->input->propbit);

	if (MT_PRESSURE)
		input_set_abs_params(ilits->input, ABS_MT_PRESSURE, 0, 255, 0, 0);

	if (MT_B_TYPE) {
#if KERNEL_VERSION(3, 7, 0) <= LINUX_VERSION_CODE
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM, INPUT_MT_DIRECT);
#else
		input_mt_init_slots(ilits->input, MAX_TOUCH_NUM);
#endif /* LINUX_VERSION_CODE */
	} else {
		input_set_abs_params(ilits->input, ABS_MT_TRACKING_ID, 0, MAX_TOUCH_NUM, 0, 0);
	}

	/* Gesture keys register */
	input_set_capability(ilits->input, EV_KEY, KEY_POWER);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_O);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_E);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_M);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_W);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_S);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_V);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_C);
	input_set_capability(ilits->input, EV_KEY, KEY_GESTURE_F);

	__set_bit(KEY_GESTURE_POWER, ilits->input->keybit);
	__set_bit(KEY_GESTURE_UP, ilits->input->keybit);
	__set_bit(KEY_GESTURE_DOWN, ilits->input->keybit);
	__set_bit(KEY_GESTURE_LEFT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_RIGHT, ilits->input->keybit);
	__set_bit(KEY_GESTURE_O, ilits->input->keybit);
	__set_bit(KEY_GESTURE_E, ilits->input->keybit);
	__set_bit(KEY_GESTURE_M, ilits->input->keybit);
	__set_bit(KEY_GESTURE_W, ilits->input->keybit);
	__set_bit(KEY_GESTURE_S, ilits->input->keybit);
	__set_bit(KEY_GESTURE_V, ilits->input->keybit);
	__set_bit(KEY_GESTURE_Z, ilits->input->keybit);
	__set_bit(KEY_GESTURE_C, ilits->input->keybit);
	__set_bit(KEY_GESTURE_F, ilits->input->keybit);
}

#if REGULATOR_POWER
void ili_plat_regulator_power_on(bool status)
{
	ILI_INFO("%s\n", status ? "POWER ON" : "POWER OFF");

	if (status) {
		if (ilits->vdd) {
			if (regulator_enable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_enable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	} else {
		if (ilits->vdd) {
			if (regulator_disable(ilits->vdd) < 0)
				ILI_ERR("regulator_enable VDD fail\n");
		}
		if (ilits->vcc) {
			if (regulator_disable(ilits->vcc) < 0)
				ILI_ERR("regulator_enable VCC fail\n");
		}
	}
	atomic_set(&ilits->ice_stat, DISABLE);
	mdelay(5);
}

static void ilitek_plat_regulator_power_init(void)
{
	const char *vdd_name = "vdd";
	const char *vcc_name = "vcc";

	ilits->vdd = regulator_get(tpd->tpd_dev, vdd_name);
	if (ERR_ALLOC_MEM(ilits->vdd)) {
		ILI_ERR("regulator_get VDD fail\n");
		ilits->vdd = NULL;
	}

	tpd->reg = ilits->vdd;

	if (regulator_set_voltage(ilits->vdd, VDD_VOLTAGE, VDD_VOLTAGE) < 0)
		ILI_ERR("Failed to set VDD %d\n", VDD_VOLTAGE);

	ilits->vcc = regulator_get(ilits->dev, vcc_name);
	if (ERR_ALLOC_MEM(ilits->vcc)) {
		ILI_ERR("regulator_get VCC fail.\n");
		ilits->vcc = NULL;
	}
	if (regulator_set_voltage(ilits->vcc, VCC_VOLTAGE, VCC_VOLTAGE) < 0)
		ILI_ERR("Failed to set VCC %d\n", VCC_VOLTAGE);

	ili_plat_regulator_power_on(true);
}
#endif

static int ilitek_plat_gpio_register(void)
{
	int ret = 0;

	ilits->tp_int = MTK_INT_GPIO;
	ilits->tp_rst = MTK_RST_GPIO;

	ILI_INFO("TP INT: %d\n", ilits->tp_int);
	ILI_INFO("TP RESET: %d\n", ilits->tp_rst);

	if (!gpio_is_valid(ilits->tp_int)) {
		ILI_ERR("Invalid INT gpio: %d\n", ilits->tp_int);
		return -EBADR;
	}

	if (!gpio_is_valid(ilits->tp_rst)) {
		ILI_ERR("Invalid RESET gpio: %d\n", ilits->tp_rst);
		return -EBADR;
	}

	gpio_direction_input(ilits->tp_int);
	return ret;
}

void ili_irq_disable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == DISABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	disable_irq_nosync(ilits->irq_num);
	atomic_set(&ilits->irq_stat, DISABLE);
	ILI_DBG("Disable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

void ili_irq_enable(void)
{
	unsigned long flag;

	spin_lock_irqsave(&ilits->irq_spin, flag);

	if (atomic_read(&ilits->irq_stat) == ENABLE)
		goto out;

	if (!ilits->irq_num) {
		ILI_ERR("gpio_to_irq (%d) is incorrect\n", ilits->irq_num);
		goto out;
	}

	enable_irq(ilits->irq_num);
	atomic_set(&ilits->irq_stat, ENABLE);
	ILI_DBG("Enable irq success\n");

out:
	spin_unlock_irqrestore(&ilits->irq_spin, flag);
}

static irqreturn_t ilitek_plat_isr_top_half(int irq, void *dev_id)
{
	if (irq != ilits->irq_num) {
		ILI_ERR("Incorrect irq number (%d)\n", irq);
		return IRQ_NONE;
	}

	if (atomic_read(&ilits->cmd_int_check) == ENABLE) {
		atomic_set(&ilits->cmd_int_check, DISABLE);
		ILI_DBG("CMD INT detected, ignore\n");
		wake_up(&(ilits->inq));
		return IRQ_HANDLED;
	}

	if (ilits->prox_near) {
		ILI_INFO("Proximity event, ignore interrupt!\n");
		return IRQ_HANDLED;
	}

	ILI_DBG("report: %d, rst: %d, fw: %d, switch: %d, mp: %d, sleep: %d, esd: %d\n",
			ilits->report,
			atomic_read(&ilits->tp_reset),
			atomic_read(&ilits->fw_stat),
			atomic_read(&ilits->tp_sw_mode),
			atomic_read(&ilits->mp_stat),
			atomic_read(&ilits->tp_sleep),
			atomic_read(&ilits->esd_stat));

	if (!ilits->report || atomic_read(&ilits->tp_reset) ||
		atomic_read(&ilits->fw_stat) || atomic_read(&ilits->tp_sw_mode) ||
		atomic_read(&ilits->mp_stat) || atomic_read(&ilits->tp_sleep) ||
		atomic_read(&ilits->esd_stat)) {
			ILI_DBG("ignore interrupt !\n");
			return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

static irqreturn_t ilitek_plat_isr_bottom_half(int irq, void *dev_id)
{
	if (mutex_is_locked(&ilits->touch_mutex)) {
		ILI_DBG("touch is locked, ignore\n");
		return IRQ_HANDLED;
	}
	mutex_lock(&ilits->touch_mutex);
	ili_report_handler();
	mutex_unlock(&ilits->touch_mutex);
	return IRQ_HANDLED;
}

void ili_irq_unregister(void)
{
	devm_free_irq(ilits->dev, ilits->irq_num, NULL);
}

int ili_irq_register(int type)
{
	int ret = 0;
	static bool get_irq_pin;
	struct device_node *node;

	atomic_set(&ilits->irq_stat, DISABLE);

	if (get_irq_pin == false) {
		node = of_find_matching_node(NULL, touch_of_match);
		if (node)
			ilits->irq_num = irq_of_parse_and_map(node, 0);

		ILI_INFO("ilits->irq_num = %d\n", ilits->irq_num);
		get_irq_pin = true;
	}

	ret = devm_request_threaded_irq(ilits->dev, ilits->irq_num,
				ilitek_plat_isr_top_half,
				ilitek_plat_isr_bottom_half,
				type | IRQF_ONESHOT, "ilitek", NULL);

	if (type == IRQF_TRIGGER_FALLING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_FALLING\n");
	if (type == IRQF_TRIGGER_RISING)
		ILI_INFO("IRQ TYPE = IRQF_TRIGGER_RISING\n");

	if (ret != 0)
		ILI_ERR("Failed to register irq handler, irq = %d, ret = %d\n", ilits->irq_num, ret);

	atomic_set(&ilits->irq_stat, ENABLE);

	return ret;
}

static void tpd_resume(struct device *h)
{
	if (ili_sleep_handler(TP_RESUME) < 0)
		ILI_ERR("TP resume failed\n");
}

static void tpd_suspend(struct device *h)
{
	if (ili_sleep_handler(TP_DEEP_SLEEP) < 0)
		ILI_ERR("TP suspend failed\n");
}

static int ilitek_tp_pm_suspend(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM SUSPEND");
	ilits->pm_suspend = true;
	reinit_completion(&ilits->pm_completion);
	return 0;
}

static int ilitek_tp_pm_resume(struct device *dev)
{
	ILI_INFO("CALL BACK TP PM RESUME");
	ilits->pm_suspend = false;
	complete(&ilits->pm_completion);
	return 0;
}

static int ilitek_plat_probe(void)
{
	ILI_INFO("platform probe\n");

#if REGULATOR_POWER
	ilitek_plat_regulator_power_init();
#endif

	if (ilitek_plat_gpio_register() < 0)
		ILI_ERR("Register gpio failed\n");

	ili_update_tp_module_info();

	ili_irq_register(ilits->irq_tirgger_type);

	if (ili_tddi_init() < 0) {
		ILI_ERR("ILITEK Driver probe failed\n");
		ili_irq_unregister();
		return -ENODEV;
	}
	tpd_load_status = 1;
	ilits->pm_suspend = false;
	init_completion(&ilits->pm_completion);

	ILI_INFO("ILITEK Driver loaded successfully!");
	return 0;
}

static int ilitek_plat_remove(void)
{
	ILI_INFO("remove plat dev\n");
	ili_dev_remove();
	return 0;
}

static const struct dev_pm_ops tp_pm_ops = {
	.suspend = ilitek_tp_pm_suspend,
	.resume = ilitek_tp_pm_resume,
};

static const struct of_device_id tp_match_table[] = {
	{.compatible = DTS_OF_NAME},
	{},
};

#ifdef ROI
struct ts_device_ops ilitek_ops = {
    .chip_roi_rawdata = ili_knuckle_roi_rawdata,
    .chip_roi_switch = ili_knuckle_roi_switch,
};
#endif

static struct ilitek_hwif_info hwif = {
	.bus_type = TDDI_INTERFACE,
	.plat_type = TP_PLAT_MTK,
	.owner = THIS_MODULE,
	.name = TDDI_DEV_ID,
	.of_match_table = of_match_ptr(tp_match_table),
	.plat_probe = ilitek_plat_probe,
	.plat_remove = ilitek_plat_remove,
	.pm = &tp_pm_ops,
};

static int tpd_local_init(void)
{
	ILI_INFO("TPD init device driver\n");

	if (ili_dev_init(&hwif) < 0) {
		ILI_ERR("Failed to register i2c/spi bus driver\n");
		return -ENODEV;
	}
	if (tpd_load_status == 0) {
		ILI_ERR("Add error touch panel driver\n");
		return -1;
	}
	if (tpd_dts_data.use_tpd_button) {
		tpd_button_setting(tpd_dts_data.tpd_key_num, tpd_dts_data.tpd_key_local,
				   tpd_dts_data.tpd_key_dim_local);
	}
	tpd_type_cap = 1;
	return 0;
}

static struct tpd_driver_t tpd_device_driver = {
	.tpd_device_name = TDDI_DEV_ID,
	.tpd_local_init = tpd_local_init,
	.suspend = tpd_suspend,
	.resume = tpd_resume,
};

static int __init ilitek_plat_dev_init(void)
{
	int ret = 0;

	ILI_INFO("ILITEK TP driver init for MTK\n");
	tpd_get_dts_info();
	ret = tpd_driver_add(&tpd_device_driver);
	if (ret < 0) {
		ILI_ERR("ILITEK add TP driver failed\n");
		tpd_driver_remove(&tpd_device_driver);
		return -ENODEV;
	}
	return 0;
}

static void __exit ilitek_plat_dev_exit(void)
{
	ILI_INFO("ilitek driver has been removed\n");
	tpd_driver_remove(&tpd_device_driver);
}

module_init(ilitek_plat_dev_init);
module_exit(ilitek_plat_dev_exit);
MODULE_AUTHOR("ILITEK");
MODULE_LICENSE("GPL");

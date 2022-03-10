// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 */
#include "fsm_public.h"
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif
#if defined(CONFIG_REGULATOR)
#include <linux/regulator/consumer.h>
static struct regulator *g_fsm_vdd;
#endif

static DEFINE_MUTEX(g_fsm_mutex);
static struct device *g_fsm_pdev;

/* customize configrature */
#include "fsm_firmware.c"
#include "fsm_class.c"
#include "fsm_misc.c"
#include "fsm_codec.c"

void fsm_mutex_lock(void)
{
	mutex_lock(&g_fsm_mutex);
}

void fsm_mutex_unlock(void)
{
	mutex_unlock(&g_fsm_mutex);
}

int fsm_i2c_reg_read(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t *pVal)
{
	struct i2c_msg msgs[2];
	uint8_t retries = 0;
	uint8_t buffer[2];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c || !pVal) {
		return -EINVAL;
	}

	// write register address.
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;
	// read register buffer.
	msgs[1].addr = fsm_dev->i2c->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 2;
	msgs[1].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter, &msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		pr_info("read %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	*pVal = ((buffer[0] << 8) | buffer[1]);

	return 0;
}

int fsm_i2c_reg_write(fsm_dev_t *fsm_dev, uint8_t reg, uint16_t val)
{
	struct i2c_msg msgs[1];
	uint8_t retries = 0;
	uint8_t buffer[3];
	int ret;

	if (!fsm_dev || !fsm_dev->i2c) {
		return -EINVAL;
	}

	buffer[0] = reg;
	buffer[1] = (val >> 8) & 0x00ff;
	buffer[2] = val & 0x00ff;
	msgs[0].addr = fsm_dev->i2c->addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(buffer);
	msgs[0].buf = &buffer[0];

	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_transfer(fsm_dev->i2c->adapter, &msgs[0], ARRAY_SIZE(msgs));
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret != ARRAY_SIZE(msgs)) {
			fsm_delay_ms(5);
			retries++;
		}
	} while (ret != ARRAY_SIZE(msgs) && retries < FSM_I2C_RETRY);

	if (ret != ARRAY_SIZE(msgs)) {
		pr_info("write %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	return 0;
}

int fsm_i2c_bulkwrite(fsm_dev_t *fsm_dev, uint8_t reg,
				uint8_t *data, int len)
{
	uint8_t retries = 0;
	uint8_t *buf;
	int size;
	int ret;

	if (!fsm_dev || !fsm_dev->i2c || !data) {
		return -EINVAL;
	}

	size = sizeof(uint8_t) + len;
	buf = (uint8_t *)fsm_alloc_mem(size);
	if (!buf) {
		pr_info("alloc memery failed");
		return -ENOMEM;
	}

	buf[0] = reg;
	memcpy(&buf[1], data, len);
	do {
		mutex_lock(&fsm_dev->i2c_lock);
		ret = i2c_master_send(fsm_dev->i2c, buf, size);
		mutex_unlock(&fsm_dev->i2c_lock);
		if (ret < 0) {
			fsm_delay_ms(5);
			retries++;
		} else if (ret == size) {
			break;
		}
	} while (ret != size && retries < FSM_I2C_RETRY);

	fsm_free_mem(buf);

	if (ret != size) {
		pr_info("write %02x transfer error: %d", reg, ret);
		return -EIO;
	}

	return 0;
}

bool fsm_set_pdev(struct device *dev)
{
	if (g_fsm_pdev == NULL || dev == NULL) {
		g_fsm_pdev = dev;
		return true;
	}
	return false; // already got device
}

struct device *fsm_get_pdev(void)
{
	return g_fsm_pdev;
}

int fsm_vddd_on(struct device *dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int ret = 0;

	if (!cfg || cfg->vddd_on) {
		return 0;
	}
#if defined(CONFIG_REGULATOR)
	g_fsm_vdd = regulator_get(dev, "fsm_vddd");
	if (IS_ERR(g_fsm_vdd) != 0) {
		pr_info("error getting fsm_vddd regulator");
		ret = PTR_ERR(g_fsm_vdd);
		g_fsm_vdd = NULL;
		return ret;
	}
	pr_info("enable regulator");
	regulator_set_voltage(g_fsm_vdd, 1800000, 1800000);
	ret = regulator_enable(g_fsm_vdd);
	if (ret < 0) {
		pr_info("enabling fsm_vddd failed: %d", ret);
	}
#endif
	cfg->vddd_on = 1;
	fsm_delay_ms(10);

	return ret;
}

void fsm_vddd_off(void)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg || !cfg->vddd_on || cfg->dev_count > 0) {
		return;
	}
#if defined(CONFIG_REGULATOR)
	if (g_fsm_vdd) {
		pr_info("disable regulator");
		regulator_disable(g_fsm_vdd);
		regulator_put(g_fsm_vdd);
		g_fsm_vdd = NULL;
	}
#endif
	cfg->vddd_on = 0;
}

void *fsm_devm_kstrdup(struct device *dev, void *buf, size_t size)
{
	char *devm_buf = devm_kzalloc(dev, size + 1, GFP_KERNEL);

	if (!devm_buf) {
		return devm_buf;
	}
	memcpy(devm_buf, buf, size);

	return devm_buf;
}

int fsm_set_monitor(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();
	int use_irq;

	if (!cfg || !fsm_dev || !fsm_dev->fsm_wq) {
		return -EINVAL;
	}
	use_irq = fsm_dev->use_irq;
	if (cfg->skip_monitor) {
		if (use_irq && fsm_dev->irq_id > 0) {
			disable_irq(fsm_dev->irq_id);
		} else {
			if (delayed_work_pending(&fsm_dev->monitor_work)) {
				cancel_delayed_work_sync(&fsm_dev->monitor_work);
			}
		}
	} else {
		if (use_irq && fsm_dev->irq_id > 0) {
			enable_irq(fsm_dev->irq_id);
		} else {
			queue_delayed_work(fsm_dev->fsm_wq,
					&fsm_dev->monitor_work, 5*HZ);
		}
	}

	return 0;
}

static int fsm_ext_reset(fsm_dev_t *fsm_dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (cfg == NULL || fsm_dev == NULL) {
		return 0;
	}
	if (cfg->reset_chip) {
		return 0;
	}
	if (fsm_dev && gpio_is_valid(fsm_dev->rst_gpio)) {
		gpio_set_value_cansleep(fsm_dev->rst_gpio, 0);
		fsm_delay_ms(10); // mdelay
		gpio_set_value_cansleep(fsm_dev->rst_gpio, 1);
		fsm_delay_ms(1); // mdelay
		cfg->reset_chip = true;
	}

	return 0;
}

static irqreturn_t fsm_irq_hander(int irq, void *data)
{
	fsm_dev_t *fsm_dev = data;

	queue_delayed_work(fsm_dev->fsm_wq, &fsm_dev->interrupt_work, 0);

	return IRQ_HANDLED;
}

static void fsm_work_monitor(struct work_struct *work)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;

	fsm_dev = container_of(work, struct fsm_dev, monitor_work.work);
	if (!cfg || cfg->skip_monitor || !fsm_dev) {
		return;
	}
	fsm_mutex_lock();
	fsm_dev_recover(fsm_dev);
	fsm_mutex_unlock();
	/* reschedule */
	queue_delayed_work(fsm_dev->fsm_wq, &fsm_dev->monitor_work,
			2*HZ);

}

static void fsm_work_interrupt(struct work_struct *work)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;

	fsm_mutex_lock();
	fsm_dev = container_of(work, struct fsm_dev, interrupt_work.work);
	if (!cfg || cfg->skip_monitor || !fsm_dev) {
		fsm_mutex_unlock();
		return;
	}
	fsm_dev_recover(fsm_dev);

	fsm_mutex_unlock();
}

static int fsm_request_irq(fsm_dev_t *fsm_dev)
{
	struct i2c_client *i2c;
	int irq_flags;
	int ret;

	if (fsm_dev == NULL || fsm_dev->i2c == NULL) {
		return -EINVAL;
	}
	fsm_dev->irq_id = -1;
	if (!fsm_dev->use_irq || !gpio_is_valid(fsm_dev->irq_gpio)) {
		pr_addr(info, "skip to request irq");
		return 0;
	}
	i2c = fsm_dev->i2c;
	/* register irq handler */
	fsm_dev->irq_id = gpio_to_irq(fsm_dev->irq_gpio);
	if (fsm_dev->irq_id <= 0) {
		pr_info("invalid irq %d\n", fsm_dev->irq_id);
		return -EINVAL;
	}
	irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	ret = devm_request_threaded_irq(&i2c->dev, fsm_dev->irq_id,
				NULL, fsm_irq_hander, irq_flags, "fs16xx", fsm_dev);
	if (ret) {
		pr_info("failed to request IRQ %d: %d\n",
				fsm_dev->irq_id, ret);
		return ret;
	}
	disable_irq(fsm_dev->irq_id);

	return 0;
}

#ifdef CONFIG_OF
static int fsm_parse_dts(struct i2c_client *i2c, fsm_dev_t *fsm_dev)
{
	struct device_node *np = i2c->dev.of_node;
	char const *position;
	int ret;

	if (fsm_dev == NULL || np == NULL) {
		return -EINVAL;
	}

	fsm_dev->rst_gpio = of_get_named_gpio(np, "fsm,rst-gpio", 0);
	if (gpio_is_valid(fsm_dev->rst_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, fsm_dev->rst_gpio,
			GPIOF_OUT_INIT_LOW, "FS16XX_RST");
		if (ret)
			return ret;
	}
	fsm_dev->irq_gpio = of_get_named_gpio(np, "fsm,irq-gpio", 0);
	if (gpio_is_valid(fsm_dev->irq_gpio)) {
		ret = devm_gpio_request_one(&i2c->dev, fsm_dev->irq_gpio,
			GPIOF_OUT_INIT_LOW, "FS16XX_IRQ");
		if (ret)
			return ret;
	}

	if (of_property_read_u32(np, "fsm,re25-dft", &fsm_dev->re25_dft)) {
		fsm_dev->re25_dft = 0;
	}
	dev_info(&i2c->dev, "re25 default:%d", fsm_dev->re25_dft);

	if (of_property_read_string(np, "fsm,position", &position)) {
		fsm_dev->pos_mask = FSM_POS_MONO; // mono
		return 0;
	}
	if (!strcmp(position, "LTOP")) {
		fsm_dev->pos_mask = FSM_POS_LTOP;
	} else if (!strcmp(position, "RBTM")) {
		fsm_dev->pos_mask = FSM_POS_RBTM;
	} else if (!strcmp(position, "LBTM")) {
		fsm_dev->pos_mask = FSM_POS_LBTM;
	} else if (!strcmp(position, "RTOP")) {
		fsm_dev->pos_mask = FSM_POS_RTOP;
	} else {
		fsm_dev->pos_mask = FSM_POS_MONO;
	}

	return 0;
}
static const struct of_device_id fsm_match_tbl[] = {
	{ .compatible = "foursemi,fs16xx" },
	{},
};
#endif

int fsm_i2c_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	fsm_config_t *cfg = fsm_get_config();
	fsm_dev_t *fsm_dev;
	int ret;

	pr_debug("enter");
	if (!i2c_check_functionality(i2c->adapter, I2C_FUNC_I2C)) {
		pr_info("check I2C_FUNC_I2C failed");
		return -EIO;
	}

	fsm_dev = devm_kzalloc(&i2c->dev, sizeof(fsm_dev_t), GFP_KERNEL);
	if (fsm_dev == NULL) {
		pr_info("alloc memory fialed");
		return -ENOMEM;
	}

	memset(fsm_dev, 0, sizeof(fsm_dev_t));
	mutex_init(&fsm_dev->i2c_lock);
	fsm_dev->i2c = i2c;

#ifdef CONFIG_OF
	ret = fsm_parse_dts(i2c, fsm_dev);
	if (ret) {
		pr_info("failed to parse DTS node");
	}
#endif
#if defined(CONFIG_FSM_REGMAP)
	fsm_dev->regmap = fsm_regmap_i2c_init(i2c);
	if (fsm_dev->regmap == NULL) {
		devm_kfree(&i2c->dev, fsm_dev);
		pr_info("regmap init fialed");
		return -EINVAL;
	}
#endif

	fsm_vddd_on(&i2c->dev);
	fsm_ext_reset(fsm_dev);
	ret = fsm_probe(fsm_dev, i2c->addr);
	if (ret) {
		pr_info("detect device failed");
#if defined(CONFIG_FSM_REGMAP)
		fsm_regmap_i2c_deinit(fsm_dev->regmap);
#endif
		devm_kfree(&i2c->dev, fsm_dev);
		return ret;
	}
#if !defined(CONFIG_FSM_CODEC)
	/* it doesn't register codec, we use the first device to request firmware,
	 * all operations in misc device */
	fsm_set_pdev(&i2c->dev);
#endif
	fsm_dev->id = cfg->dev_count - 1;
	i2c_set_clientdata(i2c, fsm_dev);
	pr_addr(info, "index:%d", fsm_dev->id);
	fsm_dev->fsm_wq = create_singlethread_workqueue("fs16xx");
	INIT_DELAYED_WORK(&fsm_dev->monitor_work, fsm_work_monitor);
	INIT_DELAYED_WORK(&fsm_dev->interrupt_work, fsm_work_interrupt);
	fsm_request_irq(fsm_dev);

	fsm_dev->has_codec = (fsm_dev->id == 0) ? true : false;
	fsm_dev->has_sys = fsm_dev->has_codec;
	if (fsm_dev->has_sys) {
		fsm_sysfs_init(&i2c->dev);
	}
	if (fsm_dev->has_codec) {
		ret = dev_set_name(&i2c->dev, "fs16xx");
		if (ret < 0) {
			pr_info("dev_set_name fialed");
		}
		fsm_codec_register(&i2c->dev, fsm_dev->id);
	}

	dev_info(&i2c->dev, "i2c probe completed");

	return 0;
}

int fsm_i2c_remove(struct i2c_client *i2c)
{
	fsm_dev_t *fsm_dev = i2c_get_clientdata(i2c);

	pr_debug("enter");
	if (fsm_dev == NULL) {
		pr_info("bad parameter");
		return -EINVAL;
	}
	if (fsm_dev->fsm_wq) {
		cancel_delayed_work_sync(&fsm_dev->interrupt_work);
		cancel_delayed_work_sync(&fsm_dev->monitor_work);
		destroy_workqueue(fsm_dev->fsm_wq);
	}
#if defined(CONFIG_FSM_REGMAP)
	fsm_regmap_i2c_deinit(fsm_dev->regmap);
#endif
	if (fsm_dev->has_codec) {
		fsm_codec_unregister(&i2c->dev);
	}
	if (fsm_dev->has_sys) {
		fsm_sysfs_deinit(&i2c->dev);
	}

	fsm_remove(fsm_dev);
	fsm_vddd_off();
	if (gpio_is_valid(fsm_dev->irq_gpio)) {
		devm_gpio_free(&i2c->dev, fsm_dev->irq_gpio);
	}
	if (gpio_is_valid(fsm_dev->rst_gpio)) {
		devm_gpio_free(&i2c->dev, fsm_dev->rst_gpio);
	}
	devm_kfree(&i2c->dev, fsm_dev);
	dev_info(&i2c->dev, "i2c removed");

	return 0;
}


#ifdef CONFIG_PM_SLEEP
static int fsm_i2c_suspend(struct device *dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg) {
		return -EINVAL;
	}
	pr_info("device suspend");
	fsm_mutex_lock();
	cfg->dev_suspend = true;
	fsm_mutex_unlock();

	return 0;
}

static int fsm_i2c_resume(struct device *dev)
{
	fsm_config_t *cfg = fsm_get_config();

	if (!cfg) {
		return -EINVAL;
	}
	pr_info("device resume");
	fsm_mutex_lock();
	cfg->dev_suspend = false;
	fsm_mutex_unlock();

	return 0;
}

static const struct dev_pm_ops fsm_i2c_pm_ops = {
	.suspend_late = fsm_i2c_suspend,
	.resume_early = fsm_i2c_resume,
};
#endif /* CONFIG_PM_SLEEP */

static const struct i2c_device_id fsm_i2c_id[] = {
	{ "fs16xx", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fsm_i2c_id);

static struct i2c_driver fsm_i2c_driver = {
	.driver = {
		.name  = FSM_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM_SLEEP
		.pm = &fsm_i2c_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(fsm_match_tbl),
#endif
	},
	.probe    = fsm_i2c_probe,
	.remove   = fsm_i2c_remove,
	.id_table = fsm_i2c_id,
};

int exfsm_i2c_probe(struct i2c_client *i2c,
			const struct i2c_device_id *id)
{
	return fsm_i2c_probe(i2c, id);
}
EXPORT_SYMBOL(exfsm_i2c_probe);

int exfsm_i2c_remove(struct i2c_client *i2c)
{
	return fsm_i2c_remove(i2c);
}
EXPORT_SYMBOL(exfsm_i2c_remove);

int fsm_i2c_init(void)
{
	return i2c_add_driver(&fsm_i2c_driver);
}

void fsm_i2c_exit(void)
{
	pr_info("enter");
	i2c_del_driver(&fsm_i2c_driver);
}

#ifdef CONFIG_FSM_STUB
static int fsm_plat_probe(struct platform_device *pdev)
{
	int ret;

	if (0) { //(pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "fsm-codec-stub");
	}
	pr_info("dev_name: %s", dev_name(&pdev->dev));
	fsm_vddd_on(&pdev->dev);
	// path: /sys/i2c-fsm/
	ret = fsm_sysfs_init(&pdev->dev);
	ret = fsm_codec_register(&pdev->dev, 0);
	ret = fsm_i2c_init();
	if (ret) {
		pr_info("i2c init failed: %d", ret);
		fsm_codec_unregister(&pdev->dev);
		fsm_sysfs_deinit(&pdev->dev);
		return ret;
	}

	return 0;
}

static int fsm_plat_remove(struct platform_device *pdev)
{
	pr_debug("enter");
	fsm_codec_unregister(&pdev->dev);
	fsm_sysfs_deinit(&pdev->dev);
	fsm_i2c_exit();
	fsm_vddd_off();
	dev_info(&pdev->dev, "platform removed");

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id fsm_codec_stub_dt_match[] = {
	{ .compatible = "foursemi,fsm-codec-stub" },
	{},
};
MODULE_DEVICE_TABLE(of, fsm_codec_stub_dt_match);
#else
static struct platform_device *soc_fsm_device;
#endif

static struct platform_driver soc_fsm_driver = {
	.driver = {
		.name = "fsm-codec-stub",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = fsm_codec_stub_dt_match,
#endif
	},
	.probe = fsm_plat_probe,
	.remove = fsm_plat_remove,
};

static int fsm_stub_init(void)
{
	int ret;

#ifndef CONFIG_OF
	// soc_fsm_device = platform_device_alloc("fsm-codec-stub", -1);
	soc_fsm_device = platform_device_register_simple("fsm-codec-stub",
				-1, NULL, 0);
	if (IS_ERR(soc_fsm_device)) {
		pr_info("register device failed");
		// return -ENOMEM;
		return PTR_ERR(soc_fsm_device);
	}

	ret = platform_device_add(soc_fsm_device);
	if (ret != 0) {
		platform_device_put(soc_fsm_device);
		return ret;
	}
#endif
	ret = platform_driver_register(&soc_fsm_driver);
	if (ret) {
		pr_info("register driver failed: %d", ret);
	}

	return ret;
}

static void fsm_stub_exit(void)
{
#ifndef CONFIG_OF
	if (!IS_ERR(soc_fsm_device)) {
		platform_device_unregister(soc_fsm_device);
	}
#endif
	platform_driver_unregister(&soc_fsm_driver);
}
#endif // CONFIG_FSM_STUB

static int __init fsm_mod_init(void)
{
	int ret;

#ifdef CONFIG_FSM_STUB
	ret = fsm_stub_init();
#else
	ret = fsm_i2c_init();
#endif
	if (ret) {
		pr_info("init fail: %d", ret);
		return ret;
	}
	fsm_misc_init();
	fsm_proc_init();

	return 0;
}

static void __exit fsm_mod_exit(void)
{
	fsm_proc_deinit();
	fsm_misc_deinit();
#ifdef CONFIG_FSM_STUB
	fsm_stub_exit();
#else
	fsm_i2c_exit();
#endif
}

//module_i2c_driver(fsm_i2c_driver);
module_init(fsm_mod_init);
module_exit(fsm_mod_exit);

MODULE_AUTHOR("FourSemi SW <support@foursemi.com>");
MODULE_DESCRIPTION("FourSemi Smart PA Driver");
MODULE_VERSION(FSM_CODE_VERSION);
MODULE_ALIAS("foursemi:"FSM_DRV_NAME);
MODULE_LICENSE("GPL");
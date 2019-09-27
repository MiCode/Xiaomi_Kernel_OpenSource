/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * Mediatek MT6370 Type-C Port Control Driver
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/mt6370.h"

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#if 1 /*  #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))*/
#include <linux/sched/rt.h>
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */

/* #define DEBUG_GPIO	66 */

#define MT6370_DRV_VERSION	"2.0.1_MTK"

#define MT6370_IRQ_WAKE_TIME	(500) /* ms */

struct mt6370_chip {
	struct i2c_client *client;
	struct device *dev;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *m_dev;
#endif /* CONFIG_RT_REGMAP */
	struct semaphore io_lock;
	struct semaphore suspend_lock;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct wakeup_source irq_wake_lock;
	struct wakeup_source i2c_wake_lock;

	atomic_t poll_count;
	struct delayed_work	poll_work;

	int irq_gpio;
	int irq;
	int chip_id;
};

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(TCPC_V10_REG_VID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_DID, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TYPEC_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PD_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_PDIF_REV, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ALERT_MASK, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TCPC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_ROLE_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_CC_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_FAULT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_COMMAND, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_MSG_HDR_INFO, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DETECT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_BUF_FRAME_TYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_RX_DATA, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_BYTE_CNT, 1, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_HDR, 2, RT_VOLATILE, {});
RT_REG_DECL(TCPC_V10_REG_TX_DATA, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_PHY_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_CLK_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_CLK_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_BMC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_BMCIO_RXDZSEL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_MT_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_MT_INT, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_MT_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_BMCIO_RXDZEN, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_IDLE_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_INTRST_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_WATCHDOG_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_I2CRST_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_TTCPC_FILTER, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_DRP_TOGGLE_CYCLE, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6370_REG_DRP_DUTY_CTRL, 1, RT_VOLATILE, {});

static const rt_register_map_t mt6370_chip_regmap[] = {
	RT_REG(TCPC_V10_REG_VID),
	RT_REG(TCPC_V10_REG_PID),
	RT_REG(TCPC_V10_REG_DID),
	RT_REG(TCPC_V10_REG_TYPEC_REV),
	RT_REG(TCPC_V10_REG_PD_REV),
	RT_REG(TCPC_V10_REG_PDIF_REV),
	RT_REG(TCPC_V10_REG_ALERT),
	RT_REG(TCPC_V10_REG_ALERT_MASK),
	RT_REG(TCPC_V10_REG_POWER_STATUS_MASK),
	RT_REG(TCPC_V10_REG_FAULT_STATUS_MASK),
	RT_REG(TCPC_V10_REG_TCPC_CTRL),
	RT_REG(TCPC_V10_REG_ROLE_CTRL),
	RT_REG(TCPC_V10_REG_FAULT_CTRL),
	RT_REG(TCPC_V10_REG_POWER_CTRL),
	RT_REG(TCPC_V10_REG_CC_STATUS),
	RT_REG(TCPC_V10_REG_POWER_STATUS),
	RT_REG(TCPC_V10_REG_FAULT_STATUS),
	RT_REG(TCPC_V10_REG_COMMAND),
	RT_REG(TCPC_V10_REG_MSG_HDR_INFO),
	RT_REG(TCPC_V10_REG_RX_DETECT),
	RT_REG(TCPC_V10_REG_RX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_RX_BUF_FRAME_TYPE),
	RT_REG(TCPC_V10_REG_RX_HDR),
	RT_REG(TCPC_V10_REG_RX_DATA),
	RT_REG(TCPC_V10_REG_TRANSMIT),
	RT_REG(TCPC_V10_REG_TX_BYTE_CNT),
	RT_REG(TCPC_V10_REG_TX_HDR),
	RT_REG(TCPC_V10_REG_TX_DATA),
	RT_REG(MT6370_REG_PHY_CTRL1),
	RT_REG(MT6370_REG_PHY_CTRL3),
	RT_REG(MT6370_REG_CLK_CTRL2),
	RT_REG(MT6370_REG_CLK_CTRL3),
	RT_REG(MT6370_REG_BMC_CTRL),
	RT_REG(MT6370_REG_BMCIO_RXDZSEL),
	RT_REG(MT6370_REG_MT_STATUS),
	RT_REG(MT6370_REG_MT_INT),
	RT_REG(MT6370_REG_MT_MASK),
	RT_REG(MT6370_REG_BMCIO_RXDZEN),
	RT_REG(MT6370_REG_IDLE_CTRL),
	RT_REG(MT6370_REG_INTRST_CTRL),
	RT_REG(MT6370_REG_WATCHDOG_CTRL),
	RT_REG(MT6370_REG_I2CRST_CTRL),
	RT_REG(MT6370_REG_SWRESET),
	RT_REG(MT6370_REG_TTCPC_FILTER),
	RT_REG(MT6370_REG_DRP_TOGGLE_CYCLE),
	RT_REG(MT6370_REG_DRP_DUTY_CTRL),
};
#define MT6370_CHIP_REGMAP_SIZE ARRAY_SIZE(mt6370_chip_regmap)

#endif /* CONFIG_RT_REGMAP */

static int mt6370_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0, count = 5;

	__pm_stay_awake(&chip->i2c_wake_lock);
	down(&chip->suspend_lock);
	while (count) {
		if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
			if (ret < 0)
				count--;
			else
				goto out;
		} else {
			ret = i2c_smbus_read_byte_data(i2c, reg);
			if (ret < 0)
				count--;
			else {
				*(u8 *)dst = (u8)ret;
				goto out;
			}
		}
		udelay(100);
	}
out:
	up(&chip->suspend_lock);
	__pm_relax(&chip->i2c_wake_lock);
	return ret;
}

static int mt6370_write_device(void *client, u32 reg, int len, const void *src)
{
	const u8 *data;
	struct i2c_client *i2c = (struct i2c_client *)client;
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0, count = 5;

	__pm_stay_awake(&chip->i2c_wake_lock);
	down(&chip->suspend_lock);
	while (count) {
		if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(i2c,
							reg, len, src);
			if (ret < 0)
				count--;
			else
				goto out;
		} else {
			data = src;
			ret = i2c_smbus_write_byte_data(i2c, reg, *data);
			if (ret < 0)
				count--;
			else
				goto out;
		}
		udelay(100);
	}
out:
	up(&chip->suspend_lock);
	__pm_relax(&chip->i2c_wake_lock);
	return ret;
}

static int mt6370_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = mt6370_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "mt6370 reg read fail\n");
		return ret;
	}
	return val;
}

static int mt6370_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = mt6370_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 reg write fail\n");
	return ret;
}

static int mt6370_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = mt6370_read_device(chip->client, reg, len, dst);
#endif /* #ifdef CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 block read fail\n");
	return ret;
}

static int mt6370_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct mt6370_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = mt6370_write_device(chip->client, reg, len, src);
#endif /* #ifdef CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "mt6370 block write fail\n");
	return ret;
}

static int32_t mt6370_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = mt6370_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t mt6370_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = mt6370_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int mt6370_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_reg_write(chip->client, reg, data);
}

static inline int mt6370_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_write_word(chip->client, reg, data);
}

static inline int mt6370_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	return mt6370_reg_read(chip->client, reg);
}

static inline int mt6370_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = mt6370_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops mt6370_regmap_fops = {
	.read_device = mt6370_read_device,
	.write_device = mt6370_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int mt6370_regmap_init(struct mt6370_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(*props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;

	props->register_num = MT6370_CHIP_REGMAP_SIZE;
	props->rm = mt6370_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE | RT_CACHE_DISABLE |
				RT_IO_PASS_THROUGH | RT_DBG_GENERAL;
	snprintf(name, sizeof(name), "mt6370-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);

	if ((!props->name) || (!props->aliases))
		return -ENOMEM;

	strlcpy((char *)props->name, name, len+1);
	strlcpy((char *)props->aliases, name, len+1);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&mt6370_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "mt6370 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int mt6370_regmap_deinit(struct mt6370_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static inline int mt6370_software_reset(struct tcpc_device *tcpc)
{
	int ret = mt6370_i2c_write8(tcpc, MT6370_REG_SWRESET, 1);

	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);
	return 0;
}

static inline int mt6370_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int mt6370_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#ifdef CONFIG_USB_POWER_DELIVERY
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return mt6370_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int mt6370_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return mt6370_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int mt6370_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return mt6370_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int mt6370_init_mt_mask(struct tcpc_device *tcpc)
{
	uint8_t mt_mask = 0;
#ifdef CONFIG_TCPC_WATCHDOG_EN
	mt_mask |= MT6370_REG_M_WATCHDOG;
#endif /* CONFIG_TCPC_WATCHDOG_EN */
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mt_mask |= MT6370_REG_M_VBUS_80;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	if (tcpc->tcpc_flags & TCPC_FLAGS_CHECK_RA_DETACHE)
		mt_mask |= MT6370_REG_M_RA_DETACH;
#endif /* CONFIG_TYPEC_CAP_RA_DETACH */

#ifdef CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG
	if (tcpc->tcpc_flags & TCPC_FLAGS_LPM_WAKEUP_WATCHDOG)
		mt_mask |= MT6370_REG_M_WAKEUP;
#endif	/* CONFIG_TYPEC_CAP_LPM_WAKEUP_WATCHDOG */

	return mt6370_i2c_write8(tcpc, MT6370_REG_MT_MASK, mt_mask);
}

static inline void mt6370_poll_ctrl(struct mt6370_chip *chip)
{
	cancel_delayed_work_sync(&chip->poll_work);

	if (atomic_read(&chip->poll_count) == 0) {
		atomic_inc(&chip->poll_count);
		cpu_idle_poll_ctrl(true);
	}

	schedule_delayed_work(
		&chip->poll_work, msecs_to_jiffies(40));
}

static void mt6370_irq_work_handler(struct kthread_work *work)
{
	struct mt6370_chip *chip =
			container_of(work, struct mt6370_chip, irq_work);
	int regval = 0;
	int gpio_val;

	mt6370_poll_ctrl(chip);
	/* make sure I2C bus had resumed */
	tcpci_lock_typec(chip->tcpc);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif
}

static void mt6370_poll_work(struct work_struct *work)
{
	struct mt6370_chip *chip = container_of(
		work, struct mt6370_chip, poll_work.work);

	if (atomic_dec_and_test(&chip->poll_count))
		cpu_idle_poll_ctrl(false);
}

static irqreturn_t mt6370_intr_handler(int irq, void *data)
{
	struct mt6370_chip *chip = data;

	__pm_wakeup_event(&chip->irq_wake_lock, MT6370_IRQ_WAKE_TIME);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 0);
#endif
	kthread_queue_work(&chip->irq_worker, &chip->irq_work);
	return IRQ_HANDLED;
}

static int mt6370_init_alert(struct tcpc_device *tcpc)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret;
	char *name;
	int len;

	/* Clear Alert Mask & Status */
	mt6370_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	mt6370_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	len = strlen(chip->tcpc_desc->name);
	name = devm_kzalloc(chip->dev, len+5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	snprintf(name, PAGE_SIZE, "%s-IRQ", chip->tcpc_desc->name);

	pr_info("%s name = %s, gpio = %d\n", __func__,
				chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
#ifdef DEBUG_GPIO
	gpio_request(DEBUG_GPIO, "debug_latency_pin");
	gpio_direction_output(DEBUG_GPIO, 1);
#endif
	if (ret < 0) {
		pr_err("Error: failed to request GPIO%d (ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		pr_err("Error: failed to set GPIO%d as input pin(ret = %d)\n",
		chip->irq_gpio, ret);
		goto init_alert_err;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq <= 0) {
		pr_err("%s gpio to irq fail, chip->irq(%d)\n",
						__func__, chip->irq);
		goto init_alert_err;
	}

	pr_info("%s : IRQ number = %d\n", __func__, chip->irq);

	kthread_init_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		goto init_alert_err;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, mt6370_irq_work_handler);

	pr_info("IRQF_NO_THREAD Test\r\n");
	ret = request_irq(chip->irq, mt6370_intr_handler,
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD |
		IRQF_NO_SUSPEND, name, chip);
	if (ret < 0) {
		pr_err("Error: failed to request irq%d (gpio = %d, ret = %d)\n",
			chip->irq, chip->irq_gpio, ret);
		goto init_alert_err;
	}

	enable_irq_wake(chip->irq);
	return 0;
init_alert_err:
	return -EINVAL;
}

int mt6370_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t mask_t2;
#endif

	/* Write 1 clear */
	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = mt6370_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = mt6370_i2c_write8(tcpc, MT6370_REG_MT_INT, mask_t2);
		if (ret < 0)
			return ret;
	}
#endif

	return 0;
}

static int mt6370_set_clock_gating(struct tcpc_device *tcpc_dev,
									bool en)
{
	int ret = 0;

#ifdef CONFIG_TCPC_CLOCK_GATING
	uint8_t clk2 = MT6370_REG_CLK_DIV_600K_EN
		| MT6370_REG_CLK_DIV_300K_EN | MT6370_REG_CLK_CK_300K_EN;

	uint8_t clk3 = MT6370_REG_CLK_DIV_2P4M_EN;

	if (!en) {
		clk2 |=
			MT6370_REG_CLK_BCLK2_EN | MT6370_REG_CLK_BCLK_EN;
		clk3 |=
			MT6370_REG_CLK_CK_24M_EN | MT6370_REG_CLK_PCLK_EN;
	}

	if (en) {
		ret = mt6370_alert_status_clear(tcpc_dev,
			TCPC_REG_ALERT_RX_STATUS |
			TCPC_REG_ALERT_RX_HARD_RST |
			TCPC_REG_ALERT_RX_BUF_OVF);
	}

	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc_dev, MT6370_REG_CLK_CTRL2, clk2);
	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc_dev, MT6370_REG_CLK_CTRL3, clk3);
#endif	/* CONFIG_TCPC_CLOCK_GATING */

	return ret;
}

static inline int mt6370_init_cc_params(
			struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

#ifdef CONFIG_USB_POWER_DELIVERY
#ifdef CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC
	uint8_t en, sel;
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	if (cc_res == TYPEC_CC_VOLT_SNK_DFT) { /* 0.55 */
		en = 1;
		sel = 0x81;
	} else if (chip->chip_id >= MT6370_DID_D) { /* 0.35 & 0.75 */
		en = 1;
		sel = 0x81;
	} else { /* 0.4 & 0.7 */
		en = 0;
		sel = 0x80;
	}

	rv = mt6370_i2c_write8(tcpc, MT6370_REG_BMCIO_RXDZEN, en);
	if (rv == 0)
		rv = mt6370_i2c_write8(tcpc, MT6370_REG_BMCIO_RXDZSEL, sel);
#endif	/* CONFIG_USB_PD_SNK_DFT_NO_GOOD_CRC */
#endif	/* CONFIG_USB_POWER_DELIVERY */

	return rv;
}

static int mt6370_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);

	MT6370_INFO("\n");

	if (sw_reset) {
		ret = mt6370_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

	/* CK_300K from 320K, SHIPPING off, AUTOIDLE enable, TIMEOUT = 32ms */
	mt6370_i2c_write8(tcpc, MT6370_REG_IDLE_CTRL,
		MT6370_REG_IDLE_SET(0, 1, 1, 2));

	/* For No-GoodCRC Case (0x70) */
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL3, 0x70);
	/* For BIST, Change Transition Toggle Counter (Noise) from 3 to 7 */
	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL1, 0x71);

#ifdef CONFIG_TCPC_I2CRST_EN
	mt6370_i2c_write8(tcpc,
		MT6370_REG_I2CRST_CTRL,
		MT6370_REG_I2CRST_SET(true, 0x0f));
#endif	/* CONFIG_TCPC_I2CRST_EN */

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	mt6370_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
		TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));

	if (chip->chip_id == MT6370_DID_A) {
		mt6370_i2c_write8(tcpc, TCPC_V10_REG_FAULT_CTRL,
			TCPC_V10_REG_FAULT_CTRL_DIS_VCONN_OV);
	}

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, based on 2.4MHz
	 * DRP Toggle Cycle : 51.2 + 6.4*val ms
	 * DRP Duyt Ctrl : dcSRC: /1024
	 */

	mt6370_i2c_write8(tcpc, MT6370_REG_TTCPC_FILTER, 5);
	mt6370_i2c_write8(tcpc, MT6370_REG_DRP_TOGGLE_CYCLE, 4);
	mt6370_i2c_write16(tcpc, MT6370_REG_DRP_DUTY_CTRL, TCPC_NORMAL_RP_DUTY);

	/* Vconn OC */
	mt6370_i2c_write8(tcpc, MT6370_REG_VCONN_CLIMITEN, 1);

	/* RX/TX Clock Gating (Auto Mode)*/
	if (!sw_reset)
		mt6370_set_clock_gating(tcpc, true);

	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	mt6370_i2c_write8(tcpc, MT6370_REG_PHY_CTRL1,
		MT6370_REG_PHY_CTRL1_SET(retry_discard_old, 7, 0, 1));

	tcpci_alert_status_clear(tcpc, 0xffffffff);

	mt6370_init_power_status_mask(tcpc);
	mt6370_init_alert_mask(tcpc);
	mt6370_init_fault_mask(tcpc);
	mt6370_init_mt_mask(tcpc);

	return 0;
}

static inline int mt6370_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, MT6370_REG_BMC_CTRL);
	if (ret < 0)
		return ret;

	ret &= ~MT6370_REG_DISCHARGE_EN;
	return mt6370_i2c_write8(tcpc, MT6370_REG_BMC_CTRL, ret);
}

static inline int mt6370_fault_status_vconn_oc(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV;

	return mt6370_i2c_write8(tcpc,
		TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

int mt6370_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = mt6370_fault_status_vconn_ov(tcpc);
	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OC)
		ret = mt6370_fault_status_vconn_oc(tcpc);

	mt6370_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int mt6370_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = mt6370_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;
	*mask = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;
#endif
	return 0;
}

int mt6370_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = mt6370_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;
#endif

	return 0;
}

static int mt6370_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;

	if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_STATUS);
	if (ret < 0)
		return ret;

	if (ret & MT6370_REG_VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#endif
	return 0;
}

int mt6370_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = mt6370_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int mt6370_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = mt6370_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = mt6370_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;

	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
	} else {
		cc_role =  TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
		if (cc_role == TYPEC_CC_RP)
			act_as_sink = false;
		else
			act_as_sink = true;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	mt6370_init_cc_params(tcpc,
		(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

static int mt6370_enable_vsafe0v_detect(
	struct tcpc_device *tcpc, bool enable)
{
	int ret = mt6370_i2c_read8(tcpc, MT6370_REG_MT_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= MT6370_REG_M_VBUS_80;
	else
		ret &= ~MT6370_REG_M_VBUS_80;

	mt6370_i2c_write8(tcpc, MT6370_REG_MT_MASK, (uint8_t) ret);
	return ret;
}

static int mt6370_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);

	MT6370_INFO("\n");
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = mt6370_i2c_write8(
			tcpc, TCPC_V10_REG_ROLE_CTRL, data);

		if (ret == 0) {
			mt6370_enable_vsafe0v_detect(tcpc, false);
			ret = mt6370_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#ifdef CONFIG_USB_POWER_DELIVERY
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			mt6370_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull, pull);
		ret = mt6370_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
	}

	return 0;
}

static int mt6370_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	data = mt6370_init_cc_params(tcpc,
		tcpc->typec_remote_cc[polarity]);
	if (data)
		return data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int mt6370_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	uint16_t duty = low_rp ? TCPC_LOW_RP_DUTY : TCPC_NORMAL_RP_DUTY;

	return mt6370_i2c_write16(tcpc, MT6370_REG_DRP_DUTY_CTRL, duty);
}

static int mt6370_set_vconn(struct tcpc_device *tcpc, int enable)
{
	int rv;
	int data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	rv = mt6370_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

#ifndef CONFIG_TCPC_IDLE_MODE
	rv = mt6370_i2c_write8(tcpc, MT6370_REG_IDLE_CTRL,
		MT6370_REG_IDLE_SET(0, 1, enable ? 0 : 1, 2));
#endif /* CONFIG_TCPC_IDLE_MODE */

	if (enable)
		mt6370_init_fault_mask(tcpc);

	return rv;
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int mt6370_is_low_power_mode(struct tcpc_device *tcpc_dev)
{
	int rv = mt6370_i2c_read8(tcpc_dev, MT6370_REG_BMC_CTRL);

	if (rv < 0)
		return rv;

	return (rv & MT6370_REG_BMCIO_LPEN) != 0;
}

static int mt6370_set_low_power_mode(
		struct tcpc_device *tcpc_dev, bool en, int pull)
{
	int rv = 0;
	uint8_t data;

	if (en) {
		data = MT6370_REG_BMCIO_LPEN;

		if (pull & TYPEC_CC_RP)
			data |= MT6370_REG_BMCIO_LPRPRD;

#ifdef CONFIG_TYPEC_CAP_NORP_SRC
		data |= (MT6370_REG_VBUS_DET_EN | MT6370_REG_BMCIO_BG_EN);
#endif	/* CONFIG_TYPEC_CAP_NORP_SRC */
	} else {
		data = MT6370_REG_BMCIO_BG_EN |
			MT6370_REG_VBUS_DET_EN | MT6370_REG_BMCIO_OSC_EN;

		mt6370_enable_vsafe0v_detect(tcpc_dev, true);
	}

	rv = mt6370_i2c_write8(tcpc_dev, MT6370_REG_BMC_CTRL, data);
	return rv;
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
int mt6370_set_watchdog(struct tcpc_device *tcpc_dev, bool en)
{
	uint8_t data = MT6370_REG_WATCHDOG_CTRL_SET(en, 7);

	return mt6370_i2c_write8(tcpc_dev,
		MT6370_REG_WATCHDOG_CTRL, data);
}
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
int mt6370_set_intrst(struct tcpc_device *tcpc_dev, bool en)
{
	return mt6370_i2c_write8(tcpc_dev,
		MT6370_REG_INTRST_CTRL, MT6370_REG_INTRST_SET(en, 3));
}
#endif	/* CONFIG_TCPC_INTRST_EN */

static int mt6370_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	mt6370_set_cc(tcpc_dev, TYPEC_CC_DRP);
	mt6370_set_cc(tcpc_dev, TYPEC_CC_OPEN);

	mt6370_i2c_write8(tcpc_dev,
		MT6370_REG_I2CRST_CTRL,
		MT6370_REG_I2CRST_SET(true, 4));

	mt6370_i2c_write8(tcpc_dev,
		MT6370_REG_INTRST_CTRL,
		MT6370_REG_INTRST_SET(true, 0));
#else
	mt6370_i2c_write8(tcpc_dev, MT6370_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */

	return 0;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int mt6370_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return mt6370_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int mt6370_protocol_reset(struct tcpc_device *tcpc_dev)
{
	mt6370_i2c_write8(tcpc_dev, MT6370_REG_PRL_FSM_RESET, 0);
	mdelay(1);
	mt6370_i2c_write8(tcpc_dev, MT6370_REG_PRL_FSM_RESET, 1);
	return 0;
}

static int mt6370_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	int ret = 0;

	if (enable)
		ret = mt6370_set_clock_gating(tcpc, false);

	if (ret == 0)
		ret = mt6370_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);

	if ((ret == 0) && (!enable))
		ret = mt6370_set_clock_gating(tcpc, true);

	/* For testing */
	if (!enable)
		mt6370_protocol_reset(tcpc);
	return ret;
}

static int mt6370_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	uint8_t type, cnt = 0;
	uint8_t buf[4];
	const uint16_t alert_rx =
		TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;

	rv = mt6370_block_read(chip->client,
			TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(uint16_t *)&buf[2];

	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (rv >= 0 && cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = mt6370_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				(uint8_t *) payload);
	}

	*frame_type = (enum tcpm_transmit_type) type;

	/* Read complete, clear RX status alert bit */
	tcpci_alert_status_clear(tcpc, alert_rx);

	/*mdelay(1); */
	return rv;
}

static int mt6370_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

/* transmit count (1byte) + message header (2byte) + data object (7*4) */
#define MT6370_TRANSMIT_MAX_SIZE (1+sizeof(uint16_t) + sizeof(uint32_t)*7)

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int mt6370_retransmit(struct tcpc_device *tcpc)
{
	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

static int mt6370_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct mt6370_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt, packet_cnt;
	uint8_t temp[MT6370_TRANSMIT_MAX_SIZE];

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(uint16_t);

		temp[0] = packet_cnt;
		memcpy(temp+1, (uint8_t *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp+3, (uint8_t *)data, data_cnt);

		rv = mt6370_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet_cnt+1, (uint8_t *)temp);
		if (rv < 0)
			return rv;
	}

	rv = mt6370_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	return rv;
}

static int mt6370_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = mt6370_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return mt6370_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops mt6370_tcpc_ops = {
	.init = mt6370_tcpc_init,
	.alert_status_clear = mt6370_alert_status_clear,
	.fault_status_clear = mt6370_fault_status_clear,
	.get_alert_mask = mt6370_get_alert_mask,
	.get_alert_status = mt6370_get_alert_status,
	.get_power_status = mt6370_get_power_status,
	.get_fault_status = mt6370_get_fault_status,
	.get_cc = mt6370_get_cc,
	.set_cc = mt6370_set_cc,
	.set_polarity = mt6370_set_polarity,
	.set_low_rp_duty = mt6370_set_low_rp_duty,
	.set_vconn = mt6370_set_vconn,
	.deinit = mt6370_tcpc_deinit,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = mt6370_is_low_power_mode,
	.set_low_power_mode = mt6370_set_low_power_mode,
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */

#ifdef CONFIG_TCPC_WATCHDOG_EN
	.set_watchdog = mt6370_set_watchdog,
#endif	/* CONFIG_TCPC_WATCHDOG_EN */

#ifdef CONFIG_TCPC_INTRST_EN
	.set_intrst = mt6370_set_intrst,
#endif	/* CONFIG_TCPC_INTRST_EN */

#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = mt6370_set_msg_header,
	.set_rx_enable = mt6370_set_rx_enable,
	.protocol_reset = mt6370_protocol_reset,
	.get_message = mt6370_get_message,
	.transmit = mt6370_transmit,
	.set_bist_test_mode = mt6370_set_bist_test_mode,
	.set_bist_carrier_mode = mt6370_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = mt6370_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */
};


static int mt_parse_dt(struct mt6370_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret;

	if (!np)
		return -EINVAL;

	pr_info("%s\n", __func__);

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		pr_err("%s find node mt6370 fail\n", __func__);
		return -ENODEV;
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "mt6370pd,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
		return ret;
	}
	chip->irq_gpio = ret;
#else
	ret = of_property_read_u32(
		np, "mt6370pd,intr_gpio_num", &chip->irq_gpio);
	if (ret < 0)
		pr_err("%s no intr_gpio info\n", __func__);
#endif
	return ret;
}

/*
 * In some platform pr_info may spend too much time on printing debug message.
 * So we use this function to test the printk performance.
 * If your platform cannot not pass this check function, please config
 * PD_DBG_INFO, this will provide the threaded debug message for you.
 */
#if TCPC_ENABLE_ANYMSG
static void check_printk_performance(void)
{
	int i;
	u64 t1, t2;
	u32 nsrem;

#ifdef CONFIG_PD_DBG_INFO
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pd_dbg_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pd_dbg_info("pd_dbg_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("pr_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_info("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_info("t2-t1 = %lu\n",
				(unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int mt6370_tcpcdev_init(struct mt6370_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np;
	u32 val, len;
	const char *name = "default";

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		pr_err("%s find node mt6370 fail\n", __func__);
		return -ENODEV;
	}

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "mt-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(
		np, "mt-tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "mt-tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case 0: /* RP Default */
			desc->rp_lvl = TYPEC_CC_RP_DFT;
			break;
		case 1: /* RP 1.5V */
			desc->rp_lvl = TYPEC_CC_RP_1_5;
			break;
		case 2: /* RP 3.0V */
			desc->rp_lvl = TYPEC_CC_RP_3_0;
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "mt-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	of_property_read_string(np, "mt-tcpc,name", (char const **)&name);

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strlcpy((char *)desc->name, name, len+1);

	chip->tcpc_desc = desc;

	chip->tcpc = tcpc_device_register(dev,
			desc, &mt6370_tcpc_ops, chip);
	if (IS_ERR(chip->tcpc))
		return -EINVAL;

	chip->tcpc->tcpc_flags =
		TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
		TCPC_FLAGS_RETRY_CRC_DISCARD;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_REV30
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */
	return 0;
}

#define MEDIATEK_6370_VID	0x29cf
#define MEDIATEK_6370_PID	0x5081

static inline int mt6370_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_VID, 2, (u8 *)&vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (vid != MEDIATEK_6370_VID) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_PID, 2, (u8 *)&pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	/* add MT6371 chip TCPC pid check for compatible */
	if (pid != MEDIATEK_6370_PID && pid != 0x5101 && pid != 0x6372) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	ret = i2c_smbus_read_i2c_block_data(client,
			MT6370_REG_SWRESET, 1, (u8 *)&data);
	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);

	ret = i2c_smbus_read_i2c_block_data(client,
			TCPC_V10_REG_DID, 2, (u8 *)&did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}

	return did;
}

static int mt6370_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mt6370_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;

	pr_info("%s\n", __func__);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	chip_id = mt6370_check_revision(client);
	if (chip_id < 0)
		return chip_id;

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt)
		mt_parse_dt(chip, &client->dev);
	else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);
	INIT_DELAYED_WORK(&chip->poll_work, mt6370_poll_work);
	wakeup_source_init(&chip->irq_wake_lock,
		"mt6370_irq_wakelock");
	wakeup_source_init(&chip->i2c_wake_lock,
		"mt6370_i2c_wakelock");

	chip->chip_id = chip_id;
	pr_info("mt6370_chipID = 0x%0x\n", chip_id);

	ret = mt6370_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "mt6370 regmap init fail\n");
		return -EINVAL;
	}

	ret = mt6370_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "mt6370 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = mt6370_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("mt6370 init alert fail\n");
		goto err_irq_init;
	}

	tcpc_schedule_init_work(chip->tcpc);
	pr_info("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	mt6370_regmap_deinit(chip);
	wakeup_source_trash(&chip->i2c_wake_lock);
	wakeup_source_trash(&chip->irq_wake_lock);
	return ret;
}

static int mt6370_i2c_remove(struct i2c_client *client)
{
	struct mt6370_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		cancel_delayed_work_sync(&chip->poll_work);

		tcpc_device_unregister(chip->dev, chip->tcpc);
		mt6370_regmap_deinit(chip);
	}

	return 0;
}

#ifdef CONFIG_PM
static int mt6370_i2c_suspend(struct device *dev)
{
	struct mt6370_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip) {
#ifdef CONFIG_USB_POWER_DELIVERY
			if (chip->tcpc->pd_wait_hard_reset_complete) {
				pr_info("%s WAITING HRESET(%d) - NO SUSPEND\n",
				    __func__,
				    chip->tcpc->pd_wait_hard_reset_complete);
				return -EAGAIN;
			}
			pr_info("%s WAIT HRESET DONE(%d) - SUSPEND\n",
				__func__,
				chip->tcpc->pd_wait_hard_reset_complete);
#endif
			down(&chip->suspend_lock);
		}
	}

	return 0;
}

static int mt6370_i2c_resume(struct device *dev)
{
	struct mt6370_chip *chip;
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}

	return 0;
}

static void mt6370_shutdown(struct i2c_client *client)
{
	struct mt6370_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
	} else {
		i2c_smbus_write_byte_data(
			client, MT6370_REG_SWRESET, 0x01);
	}
}

#ifdef CONFIG_PM_RUNTIME
static int mt6370_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int mt6370_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIME */


static const struct dev_pm_ops mt6370_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			mt6370_i2c_suspend,
			mt6370_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		mt6370_pm_suspend_runtime,
		mt6370_pm_resume_runtime,
		NULL
	)
#endif /* CONFIG_PM_RUNTIME */
};
#define MT6370_PM_OPS	(&mt6370_pm_ops)
#else
#define MT6370_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id mt6370_id_table[] = {
	{"mt6370", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6370_id_table);

static const struct of_device_id rt_match_table[] = {
	{.compatible = "mediatek,usb_type_c",},
	{},
};

static struct i2c_driver mt6370_driver = {
	.driver = {
		.name = "usb_type_c",
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = MT6370_PM_OPS,
	},
	.probe = mt6370_i2c_probe,
	.remove = mt6370_i2c_remove,
	.shutdown = mt6370_shutdown,
	.id_table = mt6370_id_table,
};

static int __init mt6370_init(void)
{
	struct device_node *np;

	pr_info("%s (%s): initializing...\n", __func__, MT6370_DRV_VERSION);
	np = of_find_node_by_name(NULL, "usb_type_c");
	if (np != NULL)
		pr_info("usb_type_c node found...\n");
	else
		pr_info("usb_type_c node not found...\n");

	return i2c_add_driver(&mt6370_driver);
}
subsys_initcall(mt6370_init);

static void __exit mt6370_exit(void)
{
	i2c_del_driver(&mt6370_driver);
}
module_exit(mt6370_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MT6370 TCPC Driver");
MODULE_VERSION(MT6370_DRV_VERSION);

/**** Release Note ****
 * 2.0.1_MTK
 *	First released PD3.0 Driver on MTK platform
 */

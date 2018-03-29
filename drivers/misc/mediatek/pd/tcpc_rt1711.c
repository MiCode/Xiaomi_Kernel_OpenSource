/*
 * Copyright (C) 2016 Richtek Technology Corp.
 *
 * drivers/misc/mediatek/pd/tcpc_rt1711.c
 * Richtek RT1711 Type-C Port Control Driver
 *
 * Author: TH <tsunghan_tasi@richtek.com>
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
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>

#include "inc/pd_dbg_info.h"
#include "inc/rt1711.h"
#include "inc/tcpci.h"

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* CONFIG_RT_REGMAP */

#if 1 /*(LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))*/
#include <linux/sched/rt.h>
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) */

#define RT1711_REDUCE_I2C_ACCESS_TIME_READ
#define RT1711_REDUCE_I2C_ACCESS_TIME_WRITE

struct rt1711_chip {
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

	int irq_gpio;
	int irq;
	int ven_id;
};

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT1711_REG_VENDOR_ID, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_ALERT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_ALERT_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_POWER_STATUS_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_CCSTATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_POWER_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_ROLECTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_POWERCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_MSGHEADERINFO, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RCVDETECT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RCVBYTECNT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RXBUFFRAMETYPE, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RXBUFHEADER, 2, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RXDATA, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TRANSMITHEADERLOWCMD, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TRANSMIT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TRANSMITBYTECNT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TXHDR, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TXDATA, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_PHYCTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_PHYCTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_PHYCTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RXTXDBG, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_LOW_POWER_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_BMCIO_RXDZSEL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_CC_EXT_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_SWRESET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TTCPCFILTER, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_DRPTOGGLECYCLE, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_DRPDUTYCTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_UNLOCKPW2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_UNLOCKPW1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_RDCAL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_OSC_FREQ, 1, RT_VOLATILE, {});
RT_REG_DECL(RT1711_REG_TESTMODE, 1, RT_VOLATILE, {});

static const rt_register_map_t rt1711_chip_regmap[] = {
	RT_REG(RT1711_REG_VENDOR_ID),
	RT_REG(RT1711_REG_ALERT),
	RT_REG(RT1711_REG_ALERT_MASK),
	RT_REG(RT1711_REG_POWER_STATUS_MASK),
	RT_REG(RT1711_REG_CCSTATUS),
	RT_REG(RT1711_REG_POWER_STATUS),
	RT_REG(RT1711_REG_ROLECTRL),
	RT_REG(RT1711_REG_POWERCTRL),
	RT_REG(RT1711_REG_MSGHEADERINFO),
	RT_REG(RT1711_REG_RCVDETECT),
	RT_REG(RT1711_REG_RCVBYTECNT),
	RT_REG(RT1711_REG_RXBUFFRAMETYPE),
	RT_REG(RT1711_REG_RXBUFHEADER),
	RT_REG(RT1711_REG_RXDATA),
	RT_REG(RT1711_REG_TRANSMITHEADERLOWCMD),
	RT_REG(RT1711_REG_TRANSMIT),
	RT_REG(RT1711_REG_TRANSMITBYTECNT),
	RT_REG(RT1711_REG_TXHDR),
	RT_REG(RT1711_REG_TXDATA),
	RT_REG(RT1711_REG_PHYCTRL2),
	RT_REG(RT1711_REG_PHYCTRL3),
	RT_REG(RT1711_REG_PHYCTRL6),
	RT_REG(RT1711_REG_RXTXDBG),
	RT_REG(RT1711_REG_LOW_POWER_CTRL),
	RT_REG(RT1711_REG_BMCIO_RXDZSEL),
	RT_REG(RT1711_REG_CC_EXT_CTRL),
	RT_REG(RT1711_REG_SWRESET),
	RT_REG(RT1711_REG_TTCPCFILTER),
	RT_REG(RT1711_REG_DRPTOGGLECYCLE),
	RT_REG(RT1711_REG_DRPDUTYCTRL),
	RT_REG(RT1711_REG_UNLOCKPW2),
	RT_REG(RT1711_REG_UNLOCKPW1),
	RT_REG(RT1711_REG_RDCAL),
	RT_REG(RT1711_REG_OSC_FREQ),
	RT_REG(RT1711_REG_TESTMODE),
};
#define RT1711_CHIP_REGMAP_SIZE ARRAY_SIZE(rt1711_chip_regmap)

#endif /* CONFIG_RT_REGMAP */

static int rt1711_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0, count = 5;

	while (count) {
		if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
			if (ret < 0)
				count--;
			else
				return ret;
		} else {
			ret = i2c_smbus_read_byte_data(i2c, reg);
			if (ret < 0)
				count--;
			else {
				*(u8 *)dst = (u8)ret;
				return ret;
			}
		}
		udelay(100);
	}
	return ret;
}

static int rt1711_write_device(void *client, u32 reg, int len, const void *src)
{
	const u8 *data;
	struct i2c_client *i2c = (struct i2c_client *)client;
	int ret = 0, count = 5;

	while (count) {
		if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(i2c,
							reg, len, src);
			if (ret < 0)
				count--;
			else
				return ret;
		} else {
			data = src;
			ret = i2c_smbus_write_byte_data(i2c, reg, *data);
			if (ret < 0)
				count--;
			else
				return ret;
		}
		udelay(100);
	}
	return ret;
}

static int rt1711_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, 1, &val);
#else
	ret = rt1711_read_device(chip->client, reg, 1, &val);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0) {
		dev_err(chip->dev, "rt1711 reg read fail\n");
		return ret;
	}
	return val;
}

static int rt1711_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, 1, &data);
#else
	ret = rt1711_write_device(chip->client, reg, 1, &data);
#endif /* CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 reg write fail\n");
	return ret;
}

#ifdef CONFIG_USB_POWER_DELIVERY
static int rt1711_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->m_dev, reg, len, dst);
#else
	ret = rt1711_read_device(chip->client, reg, len, dst);
#endif /* #ifdef CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 block read fail\n");
	return ret;
}

static int rt1711_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->m_dev, reg, len, src);
#else
	ret = rt1711_write_device(chip->client, reg, len, src);
#endif /* #ifdef CONFIG_RT_REGMAP */
	if (ret < 0)
		dev_err(chip->dev, "rt1711 block write fail\n");
	return ret;
}

#ifndef RT1711_REDUCE_I2C_ACCESS_TIME_WRITE
static int32_t rt1711_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = rt1711_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}
#endif /* RT1711_REDUCE_I2C_ACCESS_TIME_WRITE */
#endif /* CONFIG_USB_POWER_DELIVERY */

#if 0
static int rt1711_assign_bits(struct i2c_client *i2c, u8 reg,
					u8 mask, const u8 data)
{
	struct rt1711_chip *chip = i2c_get_clientdata(i2c);
	u8 value = 0;
	int ret = 0;
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd;

	ret = rt_regmap_update_bits(chip->m_dev, &rrd, reg, mask, data);
	value = 0;
#else
	down(&chip->io_lock);
	value = rt1711_reg_read(i2c, reg);
	if (value < 0) {
		up(&chip->io_lock);
		return value;
	}
	value &= ~mask;
	value |= data;
	ret = rt1711_reg_write(i2c, reg, value);
	up(&chip->io_lock);
#endif /* CONFIG_RT_REGMAP */
	return 0;
}
#endif /* #if 0 */

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops rt1711_regmap_fops = {
	.read_device = rt1711_read_device,
	.write_device = rt1711_write_device,
};
#endif /* CONFIG_RT_REGMAP */

static int rt1711_regmap_init(struct rt1711_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_properties *props;
	char name[32];
	int len;

	props = devm_kzalloc(chip->dev, sizeof(props), GFP_KERNEL);
	if (!props)
		return -ENOMEM;


	props->register_num = RT1711_CHIP_REGMAP_SIZE;
	props->rm = rt1711_chip_regmap;

	props->rt_regmap_mode = RT_MULTI_BYTE | RT_CACHE_DISABLE |
				RT_IO_PASS_THROUGH | RT_DBG_GENERAL;
	snprintf(name, 32, "rt1711-%02x", chip->client->addr);

	len = strlen(name);
	props->name = kzalloc(len+1, GFP_KERNEL);
	props->aliases = kzalloc(len+1, GFP_KERNEL);
	strcpy((char *)props->name, name);
	strcpy((char *)props->aliases, name);
	props->io_log_en = 0;

	chip->m_dev = rt_regmap_device_register(props,
			&rt1711_regmap_fops, chip->dev, chip->client, chip);
	if (!chip->m_dev) {
		dev_err(chip->dev, "rt1711 chip rt_regmap register fail\n");
		return -EINVAL;
	}
#endif
	return 0;
}

static int rt1711_regmap_deinit(struct rt1711_chip *chip)
{
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->m_dev);
#endif
	return 0;
}

static int rt1711_init_testmode(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;

#if 0	/* Enable it unless you want to write locked register */
	ret = rt1711_reg_write(chip->client, RT1711_REG_UNLOCKPW1, 0x62);
	ret |= rt1711_reg_write(chip->client, RT1711_REG_UNLOCKPW2, 0x86);
#endif

	/* For MQP CC-Noise2 Case */
	ret |= rt1711_reg_write(chip->client, RT1711_REG_PHYCTRL2, 0x3e);
	ret |= rt1711_reg_write(chip->client, RT1711_REG_BMCIO_RXDZSEL, 0x81);

	return ret;
}

static int rt1711_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	mask = RT1711_REG_ALERT_CC_STATUS | RT1711_REG_ALERT_POWER_STATUS;

#ifdef CONFIG_USB_POWER_DELIVERY
	mask |= RT1711_REG_ALERT_TX_SUCCESS | RT1711_REG_ALERT_TX_DISCARDED
		| RT1711_REG_ALERT_TX_FAILED | RT1711_REG_ALERT_RX_HARD_RST
		| RT1711_REG_ALERT_RX_STATUS;
#endif

	return rt1711_reg_write(chip->client,
		RT1711_REG_ALERT_MASK, mask);
}

static int rt1711_init_power_status_mask(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	uint8_t mask;

	mask = RT1711_VBUS_PRES_MASK;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask |= RT1711_VBUS_SAFE0V_MASK;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

	return rt1711_reg_write(chip->client,
		RT1711_REG_POWER_STATUS_MASK, mask);
}

static void rt1711_irq_work_handler(struct kthread_work *work)
{
	struct rt1711_chip *chip =
			container_of(work, struct rt1711_chip, irq_work);
	int regval = 0;
	int gpio_val;

	/* make sure I2C bus had resumed */
	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);

	do {
		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
		gpio_val = __gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);

	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);

#ifdef DEBUG_GPIO
	gpio_set_value(DEBUG_GPIO, 1);
#endif
}

static irqreturn_t rt1711_intr_handler(int irq, void *data)
{
	struct rt1711_chip *chip = data;

	queue_kthread_work(&chip->irq_worker, &chip->irq_work);
	return IRQ_HANDLED;
}

static int rt1711_init_alert(struct tcpc_device *tcpc)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	struct device_node *node;
	int ret;
	char *name;
	int len;

	len = strlen(chip->tcpc_desc->name);
	name = kzalloc(sizeof(len+5), GFP_KERNEL);
	sprintf(name, "%s-IRQ", chip->tcpc_desc->name);

	pr_info("%s name = %s\n", __func__, chip->tcpc_desc->name);

	node = of_find_node_by_name(NULL, "type_c_port0");
	if (node)
		chip->irq = irq_of_parse_and_map(node, 0);
	else
		pr_err("%s cannot get node by compatible\n", __func__);
	pr_info("chip->irq = %d\n", chip->irq);

	pr_info("%s : irq initialized...\n", __func__);
	ret = request_irq(chip->irq, rt1711_intr_handler,
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD |
		IRQF_NO_SUSPEND, name, chip);

	init_kthread_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		return -EINVAL;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	init_kthread_work(&chip->irq_work, rt1711_irq_work_handler);

	enable_irq_wake(chip->irq);

	return 0;
}

static int rt1711_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int ret;
	int power_status;

	RT1711_INFO("\n");

	if (sw_reset) {
		rt1711_reg_write(chip->client, RT1711_REG_SWRESET, 1);
		mdelay(1);
	}

	power_status = rt1711_reg_read(chip->client, RT1711_REG_POWER_STATUS);
	/* if EC Success, power_status should be 0 */
	if (power_status < 0)
		return power_status;

	ret = rt1711_init_testmode(tcpc);
	if (ret < 0)
		return ret;

	/* UFP Both RD setting */
	rt1711_reg_write(chip->client, RT1711_REG_ROLECTRL, 0x0a);

	/*
	 * CC Detect Debounce : 26.7*val us
	 * Transition window count : spec 12~20us, baseon 2.4MHz
	 * DRP Toggle Cycle : 50 + 10*val ms
	 * DRP Duty Ctrl : dcSRC: 40%
	 */

	rt1711_reg_write(chip->client, RT1711_REG_TTCPCFILTER, 0x05);
	/* For No-GoodRC Case */
	rt1711_reg_write(chip->client, RT1711_REG_PHYCTRL3, 0x70);
	rt1711_reg_write(chip->client, RT1711_REG_DRPTOGGLECYCLE, 0x02);
	rt1711_reg_write(chip->client, RT1711_REG_DRPDUTYCTRL, (1<<4)|11);

	/* Clear Rx Count */
	rt1711_reg_write(chip->client, RT1711_REG_RCVBYTECNT, 0);

	/* write 0 clear */
	rt1711_reg_write(chip->client, RT1711_REG_ALERT, 0);

	rt1711_init_alert_mask(tcpc);
	rt1711_init_power_status_mask(tcpc);

	return 0;
}

static int rt1711_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	uint16_t mask_t = (uint16_t) mask;

	/* Write 0 clear */
	return rt1711_reg_write(chip->client, RT1711_REG_ALERT, ~mask_t);
}

static int rt1711_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	/* TODO */
	return 0;
}

static int rt1711_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	int ret;

#ifdef CONFIG_TYPEC_CAP_RA_DETACH
	/* TODO: just debug code now .... 2016.1.14 TH*/

	ret = rt1711_reg_read(chip->client, RT1711_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	if (ret & RT1711_RA_DETACH_STATUS) {
		rt1711_reg_write(chip->client, RT1711_REG_CC_EXT_CTRL,
			RT1711_WAKEUP_EN | RT1711_CK_40K_SEL |
			RT1711_RA_DETACH_MASK);
	}
#endif

	ret = rt1711_reg_read(chip->client, RT1711_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = ret;
	return 0;
}

static int rt1711_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int ret;

	ret = rt1711_reg_read(chip->client, RT1711_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	*pwr_status = 0;
	if (ret & RT1711_VBUS_PRES_MASK)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	if (ret & RT1711_VBUS_SAFE0V_MASK)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#endif

	return 0;
}

static int rt1711_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	*status = 0;
	return 0;
}

#define RT1711_REG_CC_STATUS_CC1(reg)		((reg) & 0x03)
#define RT1711_REG_CC_STATUS_CC2(reg)		(((reg) & 0x0c) >> 2)
#define RT1711_REG_CC_STATUS_DRP_RESULT(reg)	(((reg) & 0x10) >> 4)

static int rt1711_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int status, role_ctrl, cc_role;
	bool act_as_sink;

	status = rt1711_reg_read(chip->client, RT1711_REG_CCSTATUS);
	if (status < 0)
		return status;

	role_ctrl = rt1711_reg_read(chip->client, RT1711_REG_ROLECTRL);
	if (role_ctrl < 0)
		return role_ctrl;

	if (status & RT1711_DRP_TOGGLING_MASK) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = RT1711_REG_CC_STATUS_CC1(status);
	*cc2 = RT1711_REG_CC_STATUS_CC2(status);

	cc_role = RT1711_REG_CC_STATUS_CC1(role_ctrl);

	switch (cc_role) {
	case TYPEC_CC_RP:
		act_as_sink = false;
		break;
	case TYPEC_CC_RD:
		act_as_sink = true;
		break;

	default:
		act_as_sink = RT1711_REG_CC_STATUS_DRP_RESULT(status);
		break;
	}

	/*
	 * If status is not open, then OR in termination to convert to
	 * enum tcpc_cc_voltage_status.
	 */

	if (*cc1 != TYPEC_CC_VOLT_OPEN)
		*cc1 |= (act_as_sink << 2);

	if (*cc2 != TYPEC_CC_VOLT_OPEN)
		*cc2 |= (act_as_sink << 2);

	return 0;
}

#define RT1711_REG_ROLE_CTRL_RES_SET(drp, rp, cc1, cc2) \
	((drp) << 6 | (rp) << 4 | (cc2) << 2 | (cc1))

static int rt1711_set_cc(struct tcpc_device *tcpc, int pull)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int ret;
	int data;
	int rp_lvl = 0;

	RT1711_INFO("\n");
	if (pull == TYPEC_CC_DRP) {
		data = rt1711_reg_read(chip->client, RT1711_REG_ROLECTRL);
		if (data < 0)
			return data;

		if (data & RT1711_ROLECTRL_DRP_MASK) {
			data = RT1711_REG_ROLE_CTRL_RES_SET(
				0, rp_lvl, TYPEC_CC_OPEN, TYPEC_CC_OPEN);

			ret = rt1711_reg_write(chip->client,
						RT1711_REG_ROLECTRL, data);
			if (ret < 0)
				return ret;
		}
		data = RT1711_REG_ROLE_CTRL_RES_SET(
			1, rp_lvl, TYPEC_CC_OPEN, TYPEC_CC_OPEN);
	} else {
		rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull);
		pull = TYPEC_CC_PULL_GET_RES(pull);

		data = RT1711_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull, pull);
	}
	return rt1711_reg_write(chip->client, RT1711_REG_ROLECTRL, data);
}

static int rt1711_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int data;

	data = rt1711_reg_read(chip->client, RT1711_REG_POWERCTRL);
	if (data < 0)
		return data;

	data &= ~RT1711_PLUG_ORIENT_MASK;
	data |= polarity ? RT1711_PLUG_ORIENT_MASK : 0;

	return rt1711_reg_write(chip->client, RT1711_REG_POWERCTRL, data);
}

static int rt1711_set_vconn(struct tcpc_device *tcpc, int enable)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int data;

	data = rt1711_reg_read(chip->client, RT1711_REG_POWERCTRL);
	if (data < 0)
		return data;

	data &= ~RT1711_VCONN_MASK;
	data |= enable ? RT1711_VCONN_MASK : 0;

	return rt1711_reg_write(chip->client, RT1711_REG_POWERCTRL, data);
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int rt1711_set_low_power_mode(
			struct tcpc_device *tcpc_dev, bool en, int pull)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc_dev);
	int rv = 0;
	uint8_t data;

	if (en) {
		data = RT1711_BMCIO_LPEN;

		if (pull & TYPEC_CC_RP)
			data = RT1711_BMCIO_LPRPRD;
	} else {
		data = RT1711_BMCIO_BG_EN |
				RT1711_REG_VBUS_DETEN | RT1711_REG_BMCIO_OSC_EN;
	}

	rv = rt1711_reg_write(chip->client, RT1711_REG_LOW_POWER_CTRL, data);
	return rv;
}
#endif	/* CONFIG_TCPC_LOW_POWER_MODE */


#ifdef CONFIG_USB_POWER_DELIVERY
#define RT1711_MSGHDR_INFO_SET(drole, prole)	\
	((prole) << 3 | (PD_REV20 << 1) | (drole))

static int rt1711_set_msg_header(
	struct tcpc_device *tcpc, int power_role, int data_role)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_write(chip->client, RT1711_REG_MSGHEADERINFO,
				RT1711_MSGHDR_INFO_SET(data_role, power_role));
}

static int rt1711_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_write(chip->client, RT1711_REG_RCVDETECT, enable);
}

static int rt1711_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int rc;
	uint8_t type, cnt = 0;
	uint8_t buf[4];

#ifdef RT1711_REDUCE_I2C_ACCESS_TIME_READ
	rc = rt1711_block_read(chip->client,
			RT1711_REG_RCVBYTECNT, 4, buf);
	cnt = buf[0];
	type = buf[1];
	*msg_head = *(uint16_t *)&buf[2];
	if (rc >= 0 && cnt > 2) {
		cnt -= 2; /* MSG_HDR */
		rc = rt1711_block_read(chip->client, RT1711_REG_RXDATA,
						cnt, (uint8_t *) payload);
	}
#else
	cnt = rt1711_reg_read(chip->client, RT1711_REG_RCVBYTECNT);
	type = rt1711_reg_read(chip->client, RT1711_REG_RXBUFFRAMETYPE);
	rc = rt1711_block_read(chip->client,
				RT1711_REG_RXBUFHEADER, 2, msg_head);

	if (rc >= 0 && cnt > 2) {
		cnt -= 2; /* MSG_HDR */
		rc = rt1711_block_read(chip->client, RT1711_REG_RXDATA,
						cnt, (uint8_t *) payload);
	}
#endif

	*frame_type = (enum tcpm_transmit_type) type;

	/* Read complete, clear RX status alert bit */
	rt1711_reg_write(chip->client, RT1711_REG_ALERT,
		~RT1711_ALERT_RXSTAT_MASK);

	/* Clear RX-Count : if we don't clear this bit, something wrong */
	rt1711_reg_write(chip->client, RT1711_REG_RCVBYTECNT, 0);

	/* mdelay(1); */
	return rc;
}

#ifdef CONFIG_USB_PD_DBG_CHECK_TXRX_BUSY
static int rt1711_check_txrx_busy(tcpc_dev_t *tcpc, int id)
{
	int busy = 0;
	int status = 0;
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	status = rt1711_reg_read(chip->client, RT1711_REG_RCVBYTECNT);
	if (status < 0)
		return status;

	if (status & RT1711_REG_RXTXDBG_TX_BUSY) {
		busy |= RT1711_REG_RXTXDBG_TX_BUSY;
		TCPC_DBG("%d:TX_BUSY!\r\n", id);
	}

	if (status & RT1711_REG_RXTXDBG_RX_BUSY) {
		busy |= RT1711_REG_RXTXDBG_RX_BUSY;
		TCPC_DBG("%d:RX_BUSY!\r\n", id);
	}

	return busy;
}
#endif

static int rt1711_set_bist_carrier_mode(
	struct tcpc_device *tcpc, uint8_t pattern)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);

	return rt1711_reg_write(chip->client,
		RT1711_REG_PHYCTRL6, (pattern)&0x7F);
}

#define RT1711_TRANSMIT_SET(type)	(PD_RETRY_COUNT << 4 | (type))

/* message header (2byte) + data object (7*4) */
#define RT1711_TRANSMIT_MAX_SIZE	(sizeof(uint16_t) + sizeof(uint32_t)*7)

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int rt1711_retransmit(struct tcpc_device *tcpc)
{
	/* TCPC 0.6 doesn't support this function */
	return 0;
}
#endif

static int rt1711_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct rt1711_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt, packet_cnt;

#ifdef RT1711_REDUCE_I2C_ACCESS_TIME_WRITE
	uint8_t temp[RT1711_TRANSMIT_MAX_SIZE];
#endif

#ifdef CONFIG_USB_PD_DBG_CHECK_TXRX_BUSY
	int i = 0;

	while (rt1711_check_txrx_busy(tcpc_dev, i)) {
		if (i++ > 5) {
			TCPC_INFO("Transmit Busy\r\n");
			break;
		}
		usleep_range(500, 1000);
	}
#endif

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = 4 * PD_HEADER_CNT(header);
		packet_cnt = data_cnt + sizeof(uint16_t);

#ifdef RT1711_REDUCE_I2C_ACCESS_TIME_WRITE
		temp[0] = packet_cnt;
		memcpy(temp+1, (uint8_t *)&header, 2);
		if (data_cnt > 0)
			memcpy(temp+3, (uint8_t *)data, data_cnt);

		rv = rt1711_block_write(chip->client,
			RT1711_REG_TRANSMITBYTECNT,
			packet_cnt+1, (uint8_t *)temp);
		if (rv < 0)
			return rv;
#else
		rv = rt1711_reg_write(chip->client,
			RT1711_REG_TRANSMITBYTECNT, packet_cnt);
		rv |= rt1711_write_word(chip->client, RT1711_REG_TXHDR, header);
		if (rv < 0)
			return rv;

		if (data_cnt > 0) {
			rv = rt1711_block_write(chip->client,
				RT1711_REG_TXDATA, data_cnt, (uint8_t *)data);
			if (rv < 0)
				return rv;
		}
#endif
	}

	rv = rt1711_reg_write(chip->client,
		RT1711_REG_TRANSMIT, RT1711_TRANSMIT_SET(type));

	return rv;
}

static int rt1711_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	/* TCPC 0.6 doesn't support this function */
	return 0;
}

#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops rt1711_tcpc_ops = {
	.init = rt1711_tcpc_init,
	.alert_status_clear = rt1711_alert_status_clear,
	.fault_status_clear = rt1711_fault_status_clear,
	.get_alert_status = rt1711_get_alert_status,
	.get_power_status = rt1711_get_power_status,
	.get_fault_status = rt1711_get_fault_status,
	.get_cc = rt1711_get_cc,
	.set_cc = rt1711_set_cc,
	.set_polarity = rt1711_set_polarity,
	.set_vconn = rt1711_set_vconn,

#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.set_low_power_mode = rt1711_set_low_power_mode,
#endif

#ifdef CONFIG_USB_POWER_DELIVERY
	.set_msg_header = rt1711_set_msg_header,
	.set_rx_enable = rt1711_set_rx_enable,
	.get_message = rt1711_get_message,
	.transmit = rt1711_transmit,
	.set_bist_test_mode = rt1711_set_bist_test_mode,
	.set_bist_carrier_mode = rt1711_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = rt1711_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

};

static int rt_parse_dt(struct rt1711_chip *chip)
{
	struct device_node *np;

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np)
		return -EINVAL;

	chip->irq_gpio = of_get_named_gpio(np, "rt1711,irq_gpio", 0);
	pr_info("%s chip->irq_gpio = %d\n", __func__, chip->irq_gpio);

	return 0;
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
		BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int rt1711_tcpcdev_init(struct rt1711_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np;
	u32 val, len;
	const char *name = "default";

	np = of_find_node_by_name(NULL, "type_c_port0");
	if (!np) {
		pr_err("%s find node rt1711h fail\n", __func__);
		return -ENODEV;
	}

	desc = devm_kzalloc(dev, sizeof(desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	if (of_property_read_u32(np, "rt-tcpc,role_def", &val) >= 0) {
		if (val >= TYPEC_ROLE_NR)
			desc->role_def = TYPEC_ROLE_DRP;
		else
			desc->role_def = val;
	} else {
		dev_info(dev, "use default Role DRP\n");
		desc->role_def = TYPEC_ROLE_DRP;
	}

	if (of_property_read_u32(
		np, "rt-tcpc,notifier_supply_num", &val) >= 0) {
		if (val < 0)
			desc->notifier_supply_num = 0;
		else
			desc->notifier_supply_num = val;
	} else
		desc->notifier_supply_num = 0;

	if (of_property_read_u32(np, "rt-tcpc,rp_level", &val) >= 0) {
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
	of_property_read_string(np, "rt-tcpc,name", (char const **)&name);

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	strcpy((char *)desc->name, name);

	chip->tcpc_desc = desc;

	chip->tcpc = tcpc_device_register(dev,
			desc, &rt1711_tcpc_ops, chip);
	if (IS_ERR(chip->tcpc))
		return -EINVAL;

	/* for RT1711H TCPC 0.6 */
#if 0
	chip->tcpc->tcpc_flags = TCPC_FLAGS_CHECK_CC_STABLE;
#else
	chip->tcpc->tcpc_flags = 0;
#endif

	return 0;
}

static int rt1711_check_i2c(struct i2c_client *i2c)
{
	int ret;
	u8 data;

	ret = rt1711_read_device(i2c, 0x12, 1, &data);
	if (ret < 0)
		return ret;
	data = 1;
	rt1711_write_device(i2c, RT1711_REG_SWRESET, 1, &data);
	msleep(20);
	return 0;
}

static int rt1711_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct rt1711_chip *chip;
	int ret = 0;
	bool use_dt = client->dev.of_node;
	u16 vendor;

	pr_info("%s\n", __func__);
	if (i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	ret = rt1711_read_device(client, RT1711_REG_VENDOR_ID, 2, &vendor);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (vendor != 0) {
		pr_info("Not Old RT1711\n");
		return -ENODEV;
	}

	ret = rt1711_check_i2c(client);
	if (ret < 0) {
		dev_err(&client->dev, "i2c fail\n");
		return -EIO;
	}

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt)
		rt_parse_dt(chip);
	else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);

	chip->ven_id = vendor;
	pr_err("chip->ven_id = %d\n", vendor);

	ret = rt1711_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "rt1711 regmap init fail\n");
		return -EINVAL;
	}

	ret = rt1711_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "rt1711 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = rt1711_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("rt1711 init alert fail\n");
		goto err_irq_init;
	}

	tcpc_schedule_init_work(chip->tcpc);
	pr_info("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	rt1711_regmap_deinit(chip);
	return ret;
}

static int rt1711_i2c_remove(struct i2c_client *client)
{
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	tcpc_device_unregister(chip->dev, chip->tcpc);
	rt1711_regmap_deinit(chip);

	return 0;
}

#ifdef CONFIG_PM
static int rt1711_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	RT1711_INFO("\n");
	down(&chip->suspend_lock);
	return 0;
}

static int rt1711_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	RT1711_INFO("\n");
	up(&chip->suspend_lock);
	return 0;
}

static void rt1711_shutdown(struct i2c_client *client)
{
	struct rt1711_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL && chip->irq)
		disable_irq(chip->irq);

	i2c_smbus_write_byte_data(client, RT1711_REG_SWRESET, 0x01);
}

#ifdef CONFIG_PM_RUNTIME
static int rt1711_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int rt1711_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* CONFIG_PM_RUNTIMR */

static const struct dev_pm_ops rt1711_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		rt1711_i2c_suspend,
		rt1711_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		rt1711_pm_suspend_runtime,
		rt1711_pm_resume_runtime,
		NULL
	)
#endif /* CONFIG_PM_RUNTIMR */
};
#define RT1711_PM_OPS	(&rt1711_pm_ops)
#else
#define RT1711_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id rt1711_id_table[] = {
	{"rt1711", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, rt1711_id_table);

static const struct of_device_id rt_match_table[] = {
	{.compatible = "richtek,rt1711",},
	{},
};

static struct i2c_driver rt1711_driver = {
	.driver = {
		.name = "rt1711",
		.owner = THIS_MODULE,
		.of_match_table = rt_match_table,
		.pm = RT1711_PM_OPS,
	},
	.probe = rt1711_i2c_probe,
	.remove = rt1711_i2c_remove,
	.shutdown = rt1711_shutdown,
	.id_table = rt1711_id_table,
};

static int __init rt1711_init(void)
{
	struct device_node *np;

	pr_info("rt1711_init() : initializing...\n");
	np = of_find_node_by_name(NULL, "usb_type_c");
	if (np != NULL)
		pr_info("rt1711 node found...\n");
	else
		pr_info("rt1711 node not found...\n");


	return i2c_add_driver(&rt1711_driver);
}
module_init(rt1711_init);

static void __exit rt1711_exit(void)
{
	i2c_del_driver(&rt1711_driver);
}
module_exit(rt1711_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("RT1711 TCPC Driver");
MODULE_VERSION("1.1.3_G");

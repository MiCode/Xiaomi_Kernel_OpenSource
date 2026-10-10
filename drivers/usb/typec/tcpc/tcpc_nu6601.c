/*
 * Copyright (C) 2022 Nuvolta Inc.
 *
 * NU6601 Type-C Port Control Driver
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
#include <linux/regmap.h>
#include <linux/semaphore.h>
#include <linux/pm_runtime.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/cpu.h>
#include <linux/version.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <linux/sched/rt.h>
#include <linux/pinctrl/consumer.h>
#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/nu6601.h"
#include "inc/tcpm.h"
#include "inc/tcpci_timer.h"

#define NU6601_DRV_VERSION	"1.0.1_NVT"
static bool tcpc_shutdown = false;

SRCU_NOTIFIER_HEAD(g_nu6601_notifier);
EXPORT_SYMBOL(g_nu6601_notifier);
struct nu6601_chip *g_tcpc_nu6601;
EXPORT_SYMBOL(g_tcpc_nu6601);

#define NU6601_IRQ_WAKE_TIME	(500) /* ms */

#define NU6601_VER_A1 (1)
#define NU6601_VER_A2 (2)
#define CONFIG_TCPC_VSAFE0V_DETECT_IC 1

enum nu6601_event {
	/*register event*/
	NU6601_EVENT_READ_TCPC_REG = 0,
	NU6601_EVENT_WRITE_TCPC_REG,

	/*cc event*/
	NU6601_EVENT_CC,

	/*com event*/
	NU6601_EVENT_DATA_INT,
	NU6601_EVENT_APEND_DATA,
	NU6601_EVENT_VER,
};

struct nu6601_evt_data {
	uint8_t reg;
	uint8_t value;

	bool is_drp;
};

struct nu6601_chip {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;
	struct tcpc_desc *tcpc_desc;
	struct tcpc_device *tcpc;
	struct notifier_block tcpc_nb;

	struct semaphore io_lock;
	struct semaphore suspend_lock;
	struct kthread_worker irq_worker;
	struct kthread_work irq_work;
	struct task_struct *irq_worker_task;
	struct wakeup_source *irq_wake_lock;

	atomic_t poll_count;
	bool cid_plug_out;
	int irq_gpio;
	int irq;
	int chip_id;
	int ver_id;

	struct pinctrl * pinctrl;
	struct pinctrl_state * old_state;
	struct pinctrl_state * new_state;
#ifdef CONFIG_TCPC_LOW_POWER_MODE
	bool in_low_power_mode;
#endif
};

static const struct regmap_config nu6601_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xFF, /* 0x80 .. 0xFF are vendor defined */
};

static int nu6601_read_device(void *client, u32 reg, int len, void *dst)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_read_i2c_block_data(i2c, reg, len, dst);
		t2 = local_clock();
		NU6601_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int nu6601_write_device(void *client, u32 reg, int len, const void *src)
{
	struct i2c_client *i2c = client;
	int ret = 0, count = 5;
	u64 t1 = 0, t2 = 0;

	while (1) {
		t1 = local_clock();
		ret = i2c_smbus_write_i2c_block_data(i2c, reg, len, src);
		t2 = local_clock();
		NU6601_INFO("%s del = %lluus, reg = %02X, len = %d\n",
			    __func__, (t2 - t1) / NSEC_PER_USEC, reg, len);
		if (ret < 0 && count > 1)
			count--;
		else
			break;
		udelay(100);
	}
	return ret;
}

static int nu6601_reg_read(struct i2c_client *i2c, u8 reg)
{
	struct nu6601_chip *chip = i2c_get_clientdata(i2c);
	u8 val = 0;
	int ret = 0;

	ret = nu6601_read_device(chip->client, reg, 1, &val);
	if (ret < 0) {
		dev_err(chip->dev, "nu6601 reg read fail\n");
		return ret;
	}
	return val;
}

static int nu6601_reg_write(struct i2c_client *i2c, u8 reg, const u8 data)
{
	struct nu6601_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;

	ret = nu6601_write_device(chip->client, reg, 1, &data);
	if (ret < 0)
		dev_err(chip->dev, "nu6601 reg write fail\n");
	return ret;
}

static int nu6601_block_read(struct i2c_client *i2c,
			u8 reg, int len, void *dst)
{
	struct nu6601_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
	ret = nu6601_read_device(chip->client, reg, len, dst);
	if (ret < 0)
		dev_err(chip->dev, "nu6601 block read fail\n");
	return ret;
}

static int nu6601_block_write(struct i2c_client *i2c,
			u8 reg, int len, const void *src)
{
	struct nu6601_chip *chip = i2c_get_clientdata(i2c);
	int ret = 0;
	ret = nu6601_write_device(chip->client, reg, len, src);
	if (ret < 0)
		dev_err(chip->dev, "nu6601 block write fail\n");
	return ret;
}

static int32_t nu6601_write_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t data)
{
	int ret;

	/* don't need swap */
	ret = nu6601_block_write(client, reg_addr, 2, (uint8_t *)&data);
	return ret;
}

static int32_t nu6601_read_word(struct i2c_client *client,
					uint8_t reg_addr, uint16_t *data)
{
	int ret;

	/* don't need swap */
	ret = nu6601_block_read(client, reg_addr, 2, (uint8_t *)data);
	return ret;
}

static inline int nu6601_i2c_write8(
	struct tcpc_device *tcpc, u8 reg, const u8 data)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	return nu6601_reg_write(chip->client, reg, data);
}

static inline int nu6601_i2c_write16(
		struct tcpc_device *tcpc, u8 reg, const u16 data)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	return nu6601_write_word(chip->client, reg, data);
}

static inline int nu6601_i2c_read8(struct tcpc_device *tcpc, u8 reg)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	return nu6601_reg_read(chip->client, reg);
}

static inline int nu6601_i2c_read16(
	struct tcpc_device *tcpc, u8 reg)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	u16 data;
	int ret;

	ret = nu6601_read_word(chip->client, reg, &data);
	if (ret < 0)
		return ret;
	return data;
}

static int nu6601_regmap_init(struct nu6601_chip *chip)
{
	chip->regmap = devm_regmap_init_i2c(chip->client,
						 &nu6601_regmap_config);
	if (IS_ERR(chip->regmap))
		return PTR_ERR(chip->regmap);

	return 0;
}

static int nu6601_pd_enable(struct tcpc_device *tcpc, int enable)
{
	uint8_t pd_en = nu6601_i2c_read8(tcpc,NU6601_REG_ANA_CTRL1);
	if(enable) {
		nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL1, (pd_en&0xfd)|0x03);
	} else {
		nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL1, (pd_en&0xfd)|0x01);
	}
	return 0;
}

static int nu6601_vbus_det_ctrl(struct tcpc_device *tcpc)
{
	uint8_t val_temp = nu6601_i2c_read8(tcpc, NU6601_REG_VBUS_DET_CTRL);

	nu6601_i2c_write8(tcpc, NU6601_REG_VBUS_DET_CTRL, ((val_temp & 0xc0) | 0x6));//3.7v

	val_temp = nu6601_i2c_read8(tcpc, NU6601_REG_VBUS_DET_CTRL);

	return 0;
}

static int nu6601_regmap_deinit(struct nu6601_chip *chip)
{
	return 0;
}

static inline int nu6601_software_reset(struct tcpc_device *tcpc)
{
	int ret = nu6601_i2c_write8(tcpc, NU6601_REG_SOFT_RST_CTRL, 1);

	if (ret < 0)
		return ret;

	usleep_range(1000, 2000);
	return 0;
}

static inline int nu6601_command(struct tcpc_device *tcpc, uint8_t cmd)
{
	return nu6601_i2c_write8(tcpc, TCPC_V10_REG_COMMAND, cmd);
}

static int nu6601_init_alert_mask(struct tcpc_device *tcpc)
{
	uint16_t mask;
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	/* Need to handle RX overflow */
	mask |= TCPC_V10_REG_ALERT_TX_SUCCESS | TCPC_V10_REG_ALERT_TX_DISCARDED
			| TCPC_V10_REG_ALERT_TX_FAILED
			| TCPC_V10_REG_ALERT_RX_HARD_RST
			| TCPC_V10_REG_ALERT_RX_STATUS
			| TCPC_V10_REG_RX_OVERFLOW;
#endif

	mask |= TCPC_REG_ALERT_FAULT;

	return nu6601_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, mask);
}

static int nu6601_init_power_status_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask = TCPC_V10_REG_POWER_STATUS_VBUS_PRES;

	return nu6601_i2c_write8(tcpc,
			TCPC_V10_REG_POWER_STATUS_MASK, mask);
}

static int nu6601_init_fault_mask(struct tcpc_device *tcpc)
{
	const uint8_t mask =
		TCPC_V10_REG_FAULT_STATUS_VCONN_OV |
		TCPC_V10_REG_FAULT_STATUS_VCONN_OC;

	return nu6601_i2c_write8(tcpc,
			TCPC_V10_REG_FAULT_STATUS_MASK, mask);
}

static int nu6601_init_nvt_mask(struct tcpc_device *tcpc)
{
	uint8_t nvt_mask = 0;
	int ret;
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	nvt_mask |= M_VBUS_80;
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */

	/*  unmask INT_FSM */
	ret = nu6601_i2c_write8(tcpc, NU6601_REG_ANA_MASK, nvt_mask);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL2, PD_AUTO_SHTD_DIS, 1);
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3, ERROR_RECOVERY, 0);
	if (ret < 0)
         return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3, CC_STBY_PWR, 0);
	if (ret < 0)
         return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 1);
	if (ret < 0)
         return ret;

	return 0;
}

static irqreturn_t nu6601_intr_handler(int irq, void *data)
{
	struct nu6601_chip *chip = data;

	pm_wakeup_event(chip->dev, NU6601_IRQ_WAKE_TIME);

 	tcpci_lock_typec(chip->tcpc);
	tcpci_alert(chip->tcpc);
 	tcpci_unlock_typec(chip->tcpc);
	return IRQ_HANDLED;
}

static int nu6601_init_alert(struct tcpc_device *tcpc)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	int ret = 0;
	char *name = NULL;
	uint8_t vref_sel;
	uint8_t en_vconn;

	/* Clear Alert Mask & Status */
	nu6601_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	nu6601_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	/* Claer Fsm Mask & Status*/
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_MASK, 0);
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, 0xff);

	//duanzx enable vconn
	vref_sel = nu6601_i2c_read8(tcpc,NU6601_REG_ANA_CTRL3);
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL3, (vref_sel&0xef)|0x10);

	en_vconn = nu6601_i2c_read8(tcpc,NU6601_REG_ANA_CTRL1);
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL1, (en_vconn&0xdf));

	name = devm_kasprintf(chip->dev, GFP_KERNEL, "%s-IRQ",
			chip->tcpc_desc->name);
	if (!name)
		return -ENOMEM;

	dev_info(chip->dev, "%s name = %s, gpio = %d\n",
			__func__, chip->tcpc_desc->name, chip->irq_gpio);

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, name);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request GPIO fail(%d)\n",
				__func__, ret);
		goto init_alert_err;
	}

	ret = gpio_direction_input(chip->irq_gpio);
	if (ret < 0) {
		dev_notice(chip->dev, "%s set GPIO fail(%d)\n", __func__, ret);
		goto init_alert_err;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		dev_notice(chip->dev, "%s gpio to irq fail(%d)",
				__func__, ret);
		goto init_alert_err;
	}

	dev_info(chip->dev, "%s IRQ number = %d\n", __func__, chip->irq);

	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					nu6601_intr_handler,
					IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					name, chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request irq fail(%d)\n",
				__func__, ret);
		goto init_alert_err;
	}
	device_init_wakeup(chip->dev, true);

	return 0;

init_alert_err:
	return -EINVAL;
}

int nu6601_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;
	uint8_t mask_t2;

	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = nu6601_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, mask_t2);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int nu6601_alert_status_clear_tx_fail(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint8_t mask_t1;
	mask_t1 = (uint8_t) mask;

	if (mask_t1) {
		ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static inline int nu6601_init_cc_params(
		struct tcpc_device *tcpc, uint8_t cc_res)
{
	int rv = 0;

	return rv;
}

static int nu6601_tcpc_init(struct tcpc_device *tcpc, bool sw_reset)
{
	int ret;
	bool retry_discard_old = false;
	tcpc->get_source_cap_flag = 0;
	tcpc->retry_time_flag = 0;
	NU6601_INFO("\n");

	if (sw_reset) {
		ret = nu6601_software_reset(tcpc);
		if (ret < 0)
			return ret;
	}

	/* UFP Both RD setting */
	/* DRP = 0, RpVal = 0 (Default), Rd, Rd */
	nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL,
			TCPC_V10_REG_ROLE_CTRL_RES_SET(0, 0, CC_RD, CC_RD));
	nu6601_i2c_write8(tcpc,0xA2,0x44);
	if (!(tcpc->tcpc_flags & TCPC_FLAGS_RETRY_CRC_DISCARD))
		retry_discard_old = true;

	nu6601_alert_status_clear(tcpc, 0xffffffff);

	nu6601_init_power_status_mask(tcpc);
	nu6601_init_alert_mask(tcpc);
	nu6601_init_fault_mask(tcpc);
	nu6601_init_nvt_mask(tcpc);

	return 0;
}

static inline int nu6601_fault_status_vconn_ov(struct tcpc_device *tcpc)
{
	int ret;

	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_CTRL1);
	if (ret < 0)
		return ret;

	ret &= ~VCONN_DISCHARGE_EN;
	return nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL1, ret);
}

static int nu6601_set_vconn(struct tcpc_device *tcpc, int enable);
int nu6601_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = nu6601_fault_status_vconn_ov(tcpc);


	nu6601_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int nu6601_get_chip_id(struct tcpc_device *tcpc, uint32_t *chip_id)
 {
    struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

  	*chip_id = chip->chip_id;

    return 0;
}

int nu6601_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	*mask = 0x2067f;

	return 0;
}

int nu6601_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
	uint8_t v2;

	ret = nu6601_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;

	*alert = (uint16_t) ret;

	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_INT);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*alert |= v2 << 16;

	return 0;
}

static int nu6601_get_power_status(
		struct tcpc_device *tcpc, uint16_t *pwr_status)
{
	int ret;

	ret = nu6601_i2c_read8(tcpc, TCPC_V10_REG_POWER_STATUS);
	if (ret < 0)
		return ret;

	//pr_err("%s power status %x\n", __func__, ret);
	*pwr_status = 0;
        if (ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES)
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;


	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_STATUS);
	if (ret < 0)
		return ret;

	if (ret & VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;

	return 0;
}

int nu6601_get_fault_status(struct tcpc_device *tcpc, uint8_t *status)
{
	int ret;

	ret = nu6601_i2c_read8(tcpc, TCPC_V10_REG_FAULT_STATUS);
	if (ret < 0)
		return ret;
	*status = (uint8_t) ret;
	return 0;
}

static int nu6601_set_cc(struct tcpc_device *tcpc, int pull);
static int nu6601_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	uint8_t digital_fsm;
	bool act_as_sink, act_as_drp;

	status = nu6601_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;


	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	if(((*cc1 == TYPEC_CC_VOLT_OPEN) && (*cc1 == TYPEC_CC_VOLT_OPEN)) && ((tcpc->typec_state != 6) && (tcpc->typec_state != 7)))
	{
		mdelay(10);
		do{
			digital_fsm = nu6601_i2c_read8(tcpc, 0xa4);
		} while(0xc == (digital_fsm&0x1f));

		if(0x3 == (digital_fsm&0x1f)) 
		{
			*cc1 = TYPEC_CC_RD; // rp3a
			*cc2 = TYPEC_CC_RD;
			nu6601_set_cc(tcpc, TYPEC_CC_RP_1_5);
		}
		else if(0x2 == (digital_fsm&0x1f))
		{
			*cc1 = 0x02;
			*cc2 = 0x02;
			nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, 0x1A);
			nu6601_set_cc(tcpc, TYPEC_CC_RD);
		}
	}

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		if(0x2 == (digital_fsm&0x1f))
			act_as_sink = true;
		else if(0x3 == (digital_fsm&0x1f))
			act_as_sink = false;
	} else {
		if (tcpc->typec_polarity)
			cc_role = TCPC_V10_REG_CC_STATUS_CC2(role_ctrl);
		else
			cc_role = TCPC_V10_REG_CC_STATUS_CC1(role_ctrl);
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

	/*pr_err("status: %x, role_ctrl: %x\n", status, role_ctrl);*/
	/*pr_err("CC1: %x, CC2: %x, act_as_sink %d, act_as_drp %d\n", *cc1, *cc2, act_as_sink, act_as_drp);*/
	nu6601_init_cc_params(tcpc,
			(uint8_t)tcpc->typec_polarity ? *cc2 : *cc1);

	return 0;
}

static int nu6601_enable_vsafe0v_detect(
		struct tcpc_device *tcpc, bool enable)
{
	int ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_MASK);

	if (ret < 0)
		return ret;

	if (enable)
		ret |= M_VBUS_80;
	else
		ret &= ~M_VBUS_80;

	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_MASK, (uint8_t) ret);
	return 0;
}

static int nu6601_set_manual_mode(struct tcpc_device *tcpc, bool enable)
{
	uint8_t ret,manual_mode;
	manual_mode = nu6601_i2c_read8(tcpc,NU6601_REG_CC_FSM_CTRL);

	if(true == enable)
	{
 		ret = nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_CTRL, (manual_mode&0xfe)|0x01);
	}
	else
	{
		ret = nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_CTRL, manual_mode&0xfe);
	}

 	if (ret < 0)
 		return ret;
	return 0;
}

static int nu6601_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data = 0, old_data = 0;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;
	uint8_t vref_sel;

	pull = TYPEC_CC_PULL_GET_RES(pull);

	old_data = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);

	if (pull == TYPEC_CC_DRP) {
		nu6601_set_manual_mode(tcpc, false);
		
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, 2, TYPEC_CC_RP, TYPEC_CC_RP);


		ret = nu6601_i2c_write8(
				tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		
		
		if (ret == 0) {

			nu6601_enable_vsafe0v_detect(tcpc, true);	// duanzx		

			ret = nu6601_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
		TCPC_INFO("drp set cc 0x%x\n", data);
	} else {

		vref_sel = nu6601_i2c_read8(tcpc,NU6601_REG_ANA_CTRL3);
		
		pull1 = pull2 = pull;

		if (pull == TYPEC_CC_RP) {
			nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL3, (vref_sel&0x9f)|0x40);
		} else if (pull == TYPEC_CC_RD) {
			nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL3, (vref_sel&0x9f));
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		TCPC_INFO("set cc 0x%x\n", data);
		if(old_data != data) {
			if (g_tcpc_nu6601->cid_plug_out)
				nu6601_set_manual_mode(tcpc, false);
			else
				nu6601_set_manual_mode(tcpc, true);
			ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
		}
	}

	return 0;
}

static int nu6601_set_polarity(struct tcpc_device *tcpc, int polarity)
{
	int data;

	data = nu6601_init_cc_params(tcpc,
		tcpc->typec_remote_cc[polarity]);
	if (data)
		return data;

	data = nu6601_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT;
	data |= polarity ? TCPC_V10_REG_TCPC_CTRL_PLUG_ORIENT : 0;
	
	return nu6601_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}

static int nu6601_set_low_rp_duty(struct tcpc_device *tcpc, bool low_rp)
{
	return 0;
}

static int nu6601_set_vconn(struct tcpc_device *tcpc, int enable)
{
	int rv;
	int data;

	data = nu6601_i2c_read8(tcpc, TCPC_V10_REG_POWER_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_POWER_CTRL_VCONN;
	data |= enable ? TCPC_V10_REG_POWER_CTRL_VCONN : 0;

	rv = nu6601_i2c_write8(tcpc, TCPC_V10_REG_POWER_CTRL, data);
	if (rv < 0)
		return rv;

	return rv;
}

static int nu6601_tcpc_deinit(struct tcpc_device *tcpc_dev)
{
#ifdef CONFIG_TCPC_SHUTDOWN_CC_DETACH
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc_dev);

	nu6601_set_cc(tcpc_dev, TYPEC_CC_DRP);
	nu6601_set_cc(tcpc_dev, TYPEC_CC_OPEN);
	regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3, ERROR_RECOVERY, 1);
	msleep(200);
	nu6601_set_cc(tcpc_dev, TYPEC_CC_RD);
	regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);

#else
	nu6601_i2c_write8(tcpc_dev, NU6601_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
	return 0;
}

#ifdef CONFIG_TCPC_LOW_POWER_MODE
static int nu6601_is_low_power_mode(struct tcpc_device *tcpc)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	if (chip != NULL)
		return chip->in_low_power_mode;

	return 0;
}

static int nu6601_set_low_power_mode(
		struct tcpc_device *tcpc, bool en, int pull)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	uint8_t data = 0;

	data = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	pr_info("%s : data:0x%x\n", __func__, data);

	if(en) {
		data &= 0xcf;//~0x30 :bi4-5 clear 0
		pr_info("%s : en low power mode:0x%x\n", __func__, data);
		if (chip != NULL)
			chip->in_low_power_mode = true;
	} else {
		data &= 0xcf;//~0x30 :bi4-5 clear 00
		data |= 0x20;//bit4-5 set 10
		pr_info("%s : dis low power mode:0x%x\n", __func__, data);
		if (chip != NULL)
			chip->in_low_power_mode = false;
	}

	return nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, data);
}
#endif

#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
static int nu6601_set_msg_header(
	struct tcpc_device *tcpc, uint8_t power_role, uint8_t data_role)
{
	uint8_t msg_hdr = TCPC_V10_REG_MSG_HDR_INFO_SET(
		data_role, power_role);

	return nu6601_i2c_write8(
		tcpc, TCPC_V10_REG_MSG_HDR_INFO, msg_hdr);
}

static int nu6601_set_rx_enable(struct tcpc_device *tcpc, uint8_t enable)
{
		return nu6601_i2c_write8(tcpc, TCPC_V10_REG_RX_DETECT, enable);
}

static int nu6601_set_bist_test_mode(struct tcpc_device *tcpc, bool en);
static int nu6601_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	int rv = 0;
	uint8_t cnt = 0, buf[4];
	uint32_t tx_payload[2] = {0x3601912c,0x3602d0c8};
	uint16_t tx_header = 0x2084;

	rv = nu6601_block_read(chip->client, TCPC_V10_REG_RX_BYTE_CNT, 4, buf);
	if (rv < 0)
		return rv;

	cnt = buf[0];
	*frame_type = buf[1];
	*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

#ifndef CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY
	if (PD_DATA_BIST == PD_HEADER_TYPE(*msg_head) && cnt > 3)
		nu6601_set_bist_test_mode(tcpc, true);
#endif /* CONFIG_USB_PD_ONLY_PRINT_SYSTEM_BUSY */
	/* TCPC 1.0 ==> no need to subtract the size of msg_head */
	if (cnt > 3) {
		cnt -= 3; /* MSG_HDR */
		rv = nu6601_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
				       payload);
	}
	tcpci_alert_status_clear(tcpc, TCPC_REG_ALERT_RX_MASK);
	if ((PD_CTRL_GET_SINK_CAP == PD_HEADER_TYPE(*msg_head) && cnt < 3) && (PD_ROLE_SOURCE == PD_HEADER_PR(*msg_head))) {
		tx_header = nu6601_i2c_read16(tcpc, TCPC_V10_REG_TX_HDR);
		tx_header = ((((tx_header & 0x0e00) >> 9) + 1) << 9) | 0x2084;
		tcpci_transmit(tcpc, TCPC_TX_SOP, tx_header, tx_payload);
	}
	return rv;
}

static int nu6601_set_bist_carrier_mode(
		struct tcpc_device *tcpc, uint8_t pattern)
{
	/* Don't support this function */
	return 0;
}

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
static int nu6601_retransmit(struct tcpc_device *tcpc)
{
	return nu6601_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, TCPC_TX_SOP));
}
#endif

#pragma pack(push, 1)
struct tcpc_transmit_packet {
	uint8_t cnt;
	uint16_t msg_header;
	uint8_t data[sizeof(uint32_t)*7];
};
#pragma pack(pop)

static int nu6601_transmit(struct tcpc_device *tcpc,
	enum tcpm_transmit_type type, uint16_t header, const uint32_t *data)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	int rv;
	int data_cnt;
	uint16_t rx_header = 0;
	int tx_flag = 0;
	int tx_cnt = 0;
	uint32_t tx_data = 0x004d02;
	struct tcpc_transmit_packet packet;

	if (type < TCPC_TX_HARD_RESET) {
		data_cnt = sizeof(uint32_t) * PD_HEADER_CNT(header);
		packet.cnt = data_cnt + sizeof(uint16_t);
		packet.msg_header = header;
		if (data_cnt > 0)
			memcpy(packet.data, (uint8_t *) data, data_cnt);

		rv = nu6601_block_write(chip->client,
				TCPC_V10_REG_TX_BYTE_CNT,
				packet.cnt+1, (uint8_t *) &packet);
		if (rv < 0)
			return rv;
	}
	if (type != TCPC_TX_HARD_RESET) {
		if (tcpc->get_source_cap_flag == 0) {
		rv = nu6601_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
				TCPC_V10_REG_TRANSMIT_SET(
				tcpc->pd_retry_count, type));
		} else {
			tcpc->get_source_cap_flag = 0;
		}
	} else {
		tx_flag = nu6601_i2c_read8(tcpc, 0x10);
		while (0x0 != (tx_flag&0x7c)) {
			mdelay(1);
			tx_flag = nu6601_i2c_read8(tcpc, 0x10);
			tx_cnt++;
			if (tx_cnt >= 4) {
				break;
			}
		};
		if (tx_cnt < 4) {
			if (tcpc->get_source_cap_flag == 0) {
			rv = nu6601_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
					TCPC_V10_REG_TRANSMIT_SET(
					tcpc->pd_retry_count, type));

			} else {
				tcpc->get_source_cap_flag = 0;
			}

		}
	}

	if (tcpc->corrupted_crc_ctrl) {
		if (PD_DATA_SINK_CAP == PD_HEADER_TYPE(header)) {
			udelay(1700);
			// tcpc_enable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
			tcpc_enable_timer(tcpc,TYPEC_TIMER_RETRY_TIME);
			tcpc->tx_snk_cap_flag = 1; 
			do {
				rx_header = nu6601_i2c_read16(tcpc, TCPC_V10_REG_RX_HDR);
				if ((PD_CTRL_GET_SOURCE_CAP == PD_HEADER_TYPE(rx_header))) {
					tcpc->get_source_cap_flag = 1;
					TCPC_INFO("slw get source cap\n");
					if (PD_ROLE_SINK == PD_HEADER_PR(rx_header)) {
						tx_data |= 0x010000;
					}
					if (PD_ROLE_UFP == PD_HEADER_DR(rx_header)) {
						tx_data |= 0x002000;
					}
					if (PD_REV30 == PD_HEADER_REV(rx_header)) {
						tx_data &= 0xffbfff;
						tx_data |= 0x008000;
					}
					nu6601_block_write(chip->client, TCPC_V10_REG_TX_BYTE_CNT, 3, (uint8_t *) &tx_data);
						tcpc_disable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
						break;
				}
				TCPC_INFO("slw tx_cnt=%d\n", tx_cnt);
				tx_cnt++;
				if (tx_cnt >= 2) {
					tcpc->get_source_cap_flag = 0;
					tcpc_disable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
					tx_cnt = 0;
					break;
				}
			} while(1);
		} else {
			tcpc->tx_snk_cap_flag = 0;
		}
	} else {
		if (PD_DATA_SINK_CAP == PD_HEADER_TYPE(header)) {
			udelay(1700);
			tcpc_enable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
			do {
				rx_header = nu6601_i2c_read16(tcpc, TCPC_V10_REG_RX_HDR);
				if ((PD_CTRL_GET_SOURCE_CAP == PD_HEADER_TYPE(rx_header))) {
					tcpc->get_source_cap_flag = 1;
					if (PD_ROLE_SINK == PD_HEADER_PR(rx_header)) {
						tx_data |= 0x010000;
					}
					if (PD_ROLE_UFP == PD_HEADER_DR(rx_header)) {
						tx_data |= 0x002000;
					}
					if (PD_REV30 == PD_HEADER_REV(rx_header)) {
						tx_data &= 0xffbfff;
						tx_data |= 0x008000;
					}
					nu6601_block_write(chip->client, TCPC_V10_REG_TX_BYTE_CNT, 3, (uint8_t *) &tx_data);
					tcpc_disable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
					break;
				}
				if (tcpc->get_source_cap_flag == 1) {
					tcpc->get_source_cap_flag = 0;
					tcpc_disable_timer(tcpc,TYPEC_TIMER_GET_SRC_HDR);
					break;
				}
			} while(1);
		}
	}

	return rv;
}

static int nu6601_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	return nu6601_i2c_write8(tcpc, 0xA2, 0x44);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static struct tcpc_ops nu6601_tcpc_ops = {
	.init = nu6601_tcpc_init,
	.alert_status_clear = nu6601_alert_status_clear,
	.alert_status_clear_tx_fail = nu6601_alert_status_clear_tx_fail,
	.fault_status_clear = nu6601_fault_status_clear,
	.get_alert_mask = nu6601_get_alert_mask,
	.get_alert_status = nu6601_get_alert_status,
	.get_power_status = nu6601_get_power_status,
	.get_fault_status = nu6601_get_fault_status,
	.get_cc = nu6601_get_cc,
	.set_cc = nu6601_set_cc,
	.set_polarity = nu6601_set_polarity,
	.set_low_rp_duty = nu6601_set_low_rp_duty,
	.set_vconn = nu6601_set_vconn,
	.deinit = nu6601_tcpc_deinit,
#ifdef CONFIG_TCPC_LOW_POWER_MODE
	.is_low_power_mode = nu6601_is_low_power_mode,
	.set_low_power_mode = nu6601_set_low_power_mode,
#endif
	.pd_enable = nu6601_pd_enable,
	.vbus_det_ctrl = nu6601_vbus_det_ctrl,
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
	.set_msg_header = nu6601_set_msg_header,
	.set_rx_enable = nu6601_set_rx_enable,
	.get_message = nu6601_get_message,
	.transmit = nu6601_transmit,
	.set_bist_test_mode = nu6601_set_bist_test_mode,
	.set_bist_carrier_mode = nu6601_set_bist_carrier_mode,
#endif	/* CONFIG_USB_POWER_DELIVERY */

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
	.retransmit = nu6601_retransmit,
#endif	/* CONFIG_USB_PD_RETRY_CRC_DISCARD */

	.get_chip_id = nu6601_get_chip_id,
};

static const char * const need_check_hwids[] = {
    "88009_1_90","88009_1_10","88009_1_11",
};

bool is_state_change_required_by_hwid(void)
{
    struct device_node *np;
    const char *hwid_str;
    const void *hwid_raw;
    char hwid[16] = {0};
    int len;

    np = of_find_node_by_path("/project-info");
    if (!np) {
        pr_err("/project-info node not found\n");
        return false;
    }

    if (!of_property_read_string(np, "hw_id", &hwid_str)) {
        strlcpy(hwid, hwid_str, sizeof(hwid));
    } else {
        hwid_raw = of_get_property(np, "hw_id", &len);
        if (!hwid_raw || len <= 0) {
            pr_err("hwid property not found or empty\n");
            return false;
        }
        //memcpy(hwid, hwid_raw, min(len, sizeof(hwid) - 1));
        memcpy(hwid, hwid_raw, min_t(size_t, len, sizeof(hwid) - 1));
    }

    pr_info(" Checking hwid = %s\n", hwid);

    for (len = 0; len < ARRAY_SIZE(need_check_hwids); len++) {
        if (!strcmp(hwid, need_check_hwids[len])) {
            pr_info("hwid %s matched, need to do state change\n", hwid);
            return true;
        }
    }

    pr_info("hwid %s not matched, skip state change\n", hwid);
    return false;
}

static int nu6601_parse_dt(struct nu6601_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct device_node *np_parent = dev->parent->of_node;
	int ret = 0;
	const char *name_1;
	const char *name_2;

	pr_info("%s\n", __func__);

	ret = of_property_read_string_index(np_parent, "pinctrl-names", 0, &name_1);
	if (ret < 0) {
          dev_err(dev->parent, "Cannot parse pinctrl-names: %d\n", ret);
          return -EINVAL;
        }
	ret = of_property_read_string_index(np_parent, "pinctrl-names", 1, &name_2);
	if (ret < 0) {
          dev_err(dev->parent, "Cannot parse pinctrl-names: %d\n", ret);
          return -EINVAL;
        }
	chip->pinctrl = devm_pinctrl_get(dev->parent);
	if (IS_ERR_OR_NULL(chip->pinctrl)) {
          dev_err(dev, "Failed to get pinctrl\n");
          return -EINVAL;
        }
	chip->old_state = pinctrl_lookup_state(chip->pinctrl, name_1);
	if (IS_ERR_OR_NULL(chip->old_state)) {
          dev_err(dev, "Failed to get pinctrl default state\n");
          return -EINVAL;
        }
	chip->new_state = pinctrl_lookup_state(chip->pinctrl, name_2);
	if (IS_ERR_OR_NULL(chip->new_state)) {
          dev_err(dev, "Failed to get pinctrl suspend state\n");
          return -EINVAL;
        }

	if (is_state_change_required_by_hwid()) {
          pr_err("Behand 88009_1_12, select old_state\n");
          ret = of_get_named_gpio(np, "rt1711pd,intr_gpio_old", 0);
          if (ret < 0) {
            pr_err("%s no intr_gpio info\n", __func__);
            return ret;
          }
          chip->irq_gpio = ret;
          ret = pinctrl_select_state(chip->pinctrl, chip->old_state);
          if (ret) {
            dev_err(dev, "Failed to select old state\n");
            return -EINVAL;
          }
	} else {
          pr_err("After 88009_1_12, select new state\n");
          ret = of_get_named_gpio(np, "rt1711pd,intr_gpio_new", 0);
          if (ret < 0) {
            pr_err("%s no intr_gpio info\n", __func__);
            return ret;
          }
          chip->irq_gpio = ret;
          ret = pinctrl_select_state(chip->pinctrl, chip->new_state);
          if (ret) {
            dev_err(dev, "Failed to select new state\n");
            return -EINVAL;
        }
    }

	return ret < 0 ? ret : 0;
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

#if IS_ENABLED(CONFIG_PD_DBG_INFO)
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
		pr_debug("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_debug("pr_info : t2-t1 = %lu\n",
				(unsigned long)nsrem / 1000);
	}
#else
	for (i = 0; i < 10; i++) {
		t1 = local_clock();
		pr_debug("%d\n", i);
		t2 = local_clock();
		t2 -= t1;
		nsrem = do_div(t2, 1000000000);
		pr_debug("t2-t1 = %lu\n",
				(unsigned long)nsrem /  1000);
		PD_BUG_ON(nsrem > 100*1000);
	}
#endif /* CONFIG_PD_DBG_INFO */
}
#endif /* TCPC_ENABLE_ANYMSG */

static int nu6601_tcpcdev_init(struct nu6601_chip *chip, struct device *dev)
{
	struct tcpc_desc *desc;
	struct device_node *np = dev->of_node;
	u32 val, len;
	const char *name = "default";

	dev_info(dev, "%s\n", __func__);

	desc = devm_kzalloc(dev, sizeof(*desc), GFP_KERNEL);
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

	if (of_property_read_u32(np, "rt-tcpc,rp_level", &val) >= 0) {
		switch (val) {
		case TYPEC_RP_DFT:
		case TYPEC_RP_1_5:
		case TYPEC_RP_3_0:
			desc->rp_lvl = val;
			break;
		default:
			break;
		}
	}

#ifdef CONFIG_TCPC_VCONN_SUPPLY_MODE
	if (of_property_read_u32(np, "rt-tcpc,vconn_supply", &val) >= 0) {
		if (val >= TCPC_VCONN_SUPPLY_NR)
			desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
		else
			desc->vconn_supply = val;
	} else {
		dev_info(dev, "use default VconnSupply\n");
		desc->vconn_supply = TCPC_VCONN_SUPPLY_ALWAYS;
	}
#endif	/* CONFIG_TCPC_VCONN_SUPPLY_MODE */

	if (of_property_read_string(np, "rt-tcpc,name",
				(char const **)&name) < 0) {
		dev_info(dev, "use default name\n");
	}

	len = strlen(name);
	desc->name = kzalloc(len+1, GFP_KERNEL);
	if (!desc->name)
		return -ENOMEM;

	strlcpy((char *)desc->name, name, len+1);

	chip->tcpc_desc = desc;

	chip->tcpc = tcpc_device_register(dev,
			desc, &nu6601_tcpc_ops, chip);
	if (IS_ERR_OR_NULL(chip->tcpc))
		return -EINVAL;

	chip->tcpc->tcpc_flags = TCPC_FLAGS_LPM_WAKEUP_WATCHDOG |
			TCPC_FLAGS_VCONN_SAFE5V_ONLY;

#ifdef CONFIG_USB_PD_RETRY_CRC_DISCARD
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_RETRY_CRC_DISCARD;
#endif  /* CONFIG_USB_PD_RETRY_CRC_DISCARD */

#ifdef CONFIG_USB_PD_REV30
		chip->tcpc->tcpc_flags |= TCPC_FLAGS_PD_REV30;

	if (chip->tcpc->tcpc_flags & TCPC_FLAGS_PD_REV30)
		dev_info(dev, "PD_REV30\n");
	else
		dev_info(dev, "PD_REV20\n");
#endif	/* CONFIG_USB_PD_REV30 */
	chip->tcpc->tcpc_flags |= TCPC_FLAGS_ALERT_V10;

	return 0;
}

static inline int nu6601_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;
	u8 data = 1;

	ret = nu6601_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	vid = 0x37A0;
	if (vid != NU6601_VID) {
		pr_debug("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = nu6601_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	pid = 0x6601;
	if (pid != NU6601_PID) {
		pr_debug("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	data = 0x03;
	ret = nu6601_write_device(client, 0x90, 1, &data);
	if (ret < 0)
		return ret;

	ret = nu6601_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}
	did = NU6601_DID_A;
	return did;
}

static int nu6601_tcpc_notify_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	uint8_t val = *((uint8_t *)data);
	dev_err(&g_tcpc_nu6601->client->dev, "event :  %ld, val %d\n", event, val);

	switch (event) {
		case NU6601_EVENT_CC:
			if (val == 1) {
				g_tcpc_nu6601->cid_plug_out = false;
				nu6601_pd_enable(g_tcpc_nu6601->tcpc, true);
				tcpm_typec_change_role(g_tcpc_nu6601->tcpc, TYPEC_ROLE_TRY_SNK);
				pr_info("cid detect plugged in");
			} else {
				g_tcpc_nu6601->cid_plug_out = true;
				nu6601_pd_enable(g_tcpc_nu6601->tcpc, false);
				tcpm_typec_change_role(g_tcpc_nu6601->tcpc, TYPEC_ROLE_SNK);
				nu6601_set_manual_mode(g_tcpc_nu6601->tcpc, false);
				pr_info("cid detect plugged out");

			}
			break;

		default:
			break;
	};

	return NOTIFY_OK;
}

static int nu6601_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct nu6601_chip *chip;
	int ret = 0, chip_id;
	bool use_dt = client->dev.of_node;

	pr_err("%s\n", __func__);
	if (i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK | I2C_FUNC_SMBUS_BYTE_DATA))
		pr_info("I2C functionality : OK...\n");
	else
		pr_info("I2C functionality check : failuare...\n");

	chip_id = nu6601_check_revision(client);
	if (chip_id < 0)
		return chip_id;

#if TCPC_ENABLE_ANYMSG
	check_printk_performance();
#endif /* TCPC_ENABLE_ANYMSG */

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	if (use_dt) {
		ret = nu6601_parse_dt(chip, &client->dev);
		if (ret < 0)
			return ret;
	} else {
		dev_err(&client->dev, "no dts node\n");
		return -ENODEV;
	}
	chip->dev = &client->dev;
	chip->client = client;
	sema_init(&chip->io_lock, 1);
	sema_init(&chip->suspend_lock, 1);
	i2c_set_clientdata(client, chip);

	chip->chip_id = chip_id;
	pr_info("nu6601_chipID = 0x%0x\n", chip_id);

	ret = nu6601_regmap_init(chip);
	if (ret < 0) {
		dev_err(chip->dev, "nu6601 regmap init fail\n");
		goto err_regmap_init;
	}

	ret = nu6601_tcpcdev_init(chip, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "nu6601 tcpc dev init fail\n");
		goto err_tcpc_reg;
	}

	ret = nu6601_init_alert(chip->tcpc);
	if (ret < 0) {
		pr_err("nu6601 init alert fail\n");
		goto err_irq_init;
	}

	chip->tcpc_nb.notifier_call = nu6601_tcpc_notify_cb;
	srcu_notifier_chain_register(&g_nu6601_notifier, &chip->tcpc_nb);

	nu6601_pd_enable(chip->tcpc,true);
	g_tcpc_nu6601 = chip;
	tcpc_shutdown = false;
	pr_err("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	nu6601_regmap_deinit(chip);
err_regmap_init:
	return ret;
}

static int nu6601_i2c_remove(struct i2c_client *client)
{
	struct nu6601_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		tcpc_device_unregister(chip->dev, chip->tcpc);
		nu6601_regmap_deinit(chip);	
		srcu_notifier_chain_unregister(&g_nu6601_notifier, &chip->tcpc_nb);
	}

	return 0;
}

#ifdef CONFIG_PM
static int nu6601_i2c_suspend(struct device *dev)
{
	struct nu6601_chip *chip = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	disable_irq(chip->irq);

	return 0;
}

static int nu6601_i2c_resume(struct device *dev)
{
	struct nu6601_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	enable_irq(chip->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);

	return 0;
}

static void nu6601_shutdown(struct i2c_client *client)
{
	struct nu6601_chip *chip = i2c_get_clientdata(client);

	/* Please reset IC here */
	if (chip != NULL) {
		if (chip->irq)
			disable_irq(chip->irq);
		tcpm_shutdown(chip->tcpc);
		tcpc_shutdown = true;
		nu6601_pd_enable(chip->tcpc,false);
	} else {
		i2c_smbus_write_byte_data(
			client, NU6601_REG_SOFT_RST_CTRL, 0x01);
	}
}

#ifdef CONFIG_PM_RUNTIME
static int nu6601_pm_suspend_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: suspending...\n");
	return 0;
}

static int nu6601_pm_resume_runtime(struct device *device)
{
	dev_dbg(device, "pm_runtime: resuming...\n");
	return 0;
}
#endif /* #ifdef CONFIG_PM_RUNTIME */

static const struct dev_pm_ops nu6601_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
			nu6601_i2c_suspend,
			nu6601_i2c_resume)
#ifdef CONFIG_PM_RUNTIME
	SET_RUNTIME_PM_OPS(
		nu6601_pm_suspend_runtime,
		nu6601_pm_resume_runtime,
		NULL
	)
#endif /* #ifdef CONFIG_PM_RUNTIME */
};
#define NU6601_PM_OPS	(&nu6601_pm_ops)
#else
#define NU6601_PM_OPS	(NULL)
#endif /* CONFIG_PM */

static const struct i2c_device_id nu6601_id_table[] = {
	{"nu6601_typec", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, nu6601_id_table);

static const struct of_device_id nvt_match_table[] = {
	{.compatible = "nuvolta,nu6601_typec_pd",},
	{},
};
MODULE_DEVICE_TABLE(of, nvt_match_table);

static struct i2c_driver nu6601_driver = {
	.driver = {
		.name = "nu6601_typec_pd",
		.owner = THIS_MODULE,
		.of_match_table = nvt_match_table,
		.pm = NU6601_PM_OPS,
	},
	.probe = nu6601_i2c_probe,
	.remove = nu6601_i2c_remove,
	.shutdown = nu6601_shutdown,
	.id_table = nu6601_id_table,
};

module_i2c_driver(nu6601_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Danny <zhanxiao.duan@nuvoltatek.com>");
MODULE_DESCRIPTION("NU6601 TCPC Driver");
MODULE_VERSION(NU6601_DRV_VERSION);

/**** Release Note ****
 * 1.0.1_NVT
 * NVT first released PD3.0 Driver
 */

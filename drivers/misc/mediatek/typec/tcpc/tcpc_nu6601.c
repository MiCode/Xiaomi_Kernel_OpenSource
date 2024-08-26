/*
 * Copyright (C) 2022 Nuvolta Inc.
 *
 * Richtek NU6601 Type-C Port Control Driver
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

#include "inc/pd_dbg_info.h"
#include "inc/tcpci.h"
#include "inc/nu6601.h"
#include "inc/tcpm.h"

#define NU6601_DRV_VERSION	"1.0.1_NVT"

static uint8_t get_data_from_int = 1;
static uint8_t apend_ps_rdy = 1;

SRCU_NOTIFIER_HEAD(g_nu6601_notifier);
EXPORT_SYMBOL(g_nu6601_notifier);
struct nu6601_chip *g_tcpc_nu6601;
EXPORT_SYMBOL(g_tcpc_nu6601);

#define NU6601_IRQ_WAKE_TIME	(500) /* ms */

#define NU6601_VER_A1 (1)
#define NU6601_VER_A2 (2)

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

	struct delayed_work attach_wait_work;
	atomic_t poll_count;

	int irq_gpio;
	int irq;
	int chip_id;
	int ver_id;
};

static const struct regmap_config nu6601_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = 0xFF, /* 0x80 .. 0xFF are vendor defined */
};

static u8 g_fsm_status = 0;
static uint16_t int_alert;
static uint16_t int_msg_cnt;
static uint16_t int_msg_head;
static uint8_t int_msg_type;
static uint8_t int_msg_data[40];

enum nu_pd_state {
	NU_PD_STATE_NONE = 0,
	NU_PD_STATE_REQUEST = 2,
	NU_PD_STATE_ACCEPT = 3,
	NU_PD_STATE_STATE,
};
enum nu_pd_state respect_state = NU_PD_STATE_NONE;
static uint8_t g_rx_id = 0;
static uint8_t ext,rev,ct,pr,dr;

const char *nu6601_cc_fsm_status[] = {
	"Unattached.SNK",
	"Unattached.SRC",
	"AttachWait.SNK",
	"AttachWait.SRC",
	"Try.SNK",
	"Try.SRC (Reserved)",
	"TryWait.SNK (Reserved)",
	"TryWait.SRC",
	"Attached.SNK",
	"Attached.SRC",
	"AudioAccessory",
	"Reserved",
	"Reserved",
	"DebugAccessory.SNK",
	"ErrorRecovery (No INT)",
	"Disabled (No INT)"
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

static int nu6601_pd_enable(struct tcpc_device *tcpc, bool enable)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	return regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, PD_BUFFER_EN,
			      enable ? PD_BUFFER_EN : 0);
}

static int nu6601_reset_cc(struct tcpc_device *tcpc)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3,
				CC_DISABLE, CC_DISABLE);
	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3,
				CC_DISABLE, 0);
	return ret;
}
static int nu6601_pd_reset(struct tcpc_device *tcpc)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	int ret = 0;
	ret |= regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL2, PD_AUTO_SHTD_DIS, PD_AUTO_SHTD_DIS);
	ret |= nu6601_pd_enable(tcpc,false);
	ret |= nu6601_pd_enable(tcpc,true);
	ret |= regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL2, PD_AUTO_SHTD_DIS, 0);
	pr_info("nu6601_pd_reset done");
	return ret;
}

static int nu6601_regmap_deinit(struct nu6601_chip *chip)
{
	return 0;
}

static inline int nu6601_software_reset(struct tcpc_device *tcpc)
{
#if 1
	int ret = nu6601_i2c_write8(tcpc, NU6601_REG_SOFT_RST_CTRL, 1);

	if (ret < 0)
		return ret;
#else
	int ret;
	ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, 0x4f);
	ret |= nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL3, 0x2a);
	ret |= nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, 0x4a);
	ret |= nu6601_i2c_write8(tcpc, NU6601_REG_ANA_CTRL3, 0x28);
	if (ret < 0)
		return ret;
#endif

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

	//mask = TCPC_V10_REG_ALERT_CC_STATUS | TCPC_V10_REG_ALERT_POWER_STATUS;
	mask = TCPC_V10_REG_ALERT_POWER_STATUS;

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

	nvt_mask |= M_FSM_STA;

	/*  unmask INT_FSM */
	ret = nu6601_i2c_write8(tcpc, NU6601_REG_ANA_MASK, nvt_mask);
	if (ret < 0)
		return ret;

	/*  unmask attached sink/src unattached sink/src */
	ret = nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_STATUS_MASK,
			~(ATTACHED_SNK_SRC_MASK | DEBUGACC_SNK_SRC_MASK | ATTACHWAIT_SNK_SRC_MASK));
	if (ret < 0)
		return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL2, PD_AUTO_SHTD_DIS, 0);
	if (ret < 0)
		return ret;

/*
 *    ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL2, TX_ALERT_IGNORE, 1);
 *    if (ret < 0)
 *        return ret;
 *
 */
	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3, ERROR_RECOVERY, 0);
	if (ret < 0)
         return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL3, CC_STBY_PWR, 0);
	if (ret < 0)
         return ret;

	ret = regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 1);
	if (ret < 0)
         return ret;

	/* set exit sink based on cc */
	//ret = regmap_update_bits(chip->regmap, NU6601_REG_CC_FSM_CTRL, EXIT_SNK_BASED_ON_CC, 1);
	//if (ret < 0)
	//	return ret;

	return 0;
}

static void nu6601_trans_machine(uint16_t header)
{
	uint8_t cnt,type;

	cnt = PD_HEADER_CNT(header);
	type = PD_HEADER_TYPE(header);

	if (cnt != 0 && type == PD_DATA_REQUEST) {
		respect_state = NU_PD_STATE_ACCEPT;
	}
}

static uint16_t nu6601_append_data(uint16_t rx_header)
{
	uint8_t msg_id,msg_type;
	uint16_t append_header = 0;

	msg_id = PD_HEADER_ID(rx_header);
	msg_type = PD_HEADER_TYPE(rx_header);

	if ((respect_state != NU_PD_STATE_NONE) && (msg_type != respect_state || (g_rx_id + 1) % PD_MSG_ID_MAX != msg_id)) {
		pr_err("err rx msg, respect_state 0x%x, g_rx_id = %d, rx_header 0x%x ,type 0x%x, msg_id %d\n", respect_state, g_rx_id, rx_header, msg_type, msg_id);
		if (respect_state == NU_PD_STATE_ACCEPT) {
			pr_err("append msg, type %d, rev %d, pr %d, dr %d, id %d, ct %d, ext %d\n", PD_CTRL_ACCEPT, rev, pr, dr, (g_rx_id + 1) % PD_MSG_ID_MAX, ct, ext);
			append_header = PD_HEADER_SOP(PD_CTRL_ACCEPT, rev, pr, dr, (g_rx_id + 1) % PD_MSG_ID_MAX, ct, ext);
			respect_state = NU_PD_STATE_NONE;
			pr_err("append header: 0x%x\n", append_header);
			msg_id = (g_rx_id + 1) % PD_MSG_ID_MAX;
		}
	} else {
		if (msg_type == PD_CTRL_ACCEPT) {
			ext = PD_HEADER_EXT(rx_header);
			rev = PD_HEADER_REV(rx_header);
			ct = PD_HEADER_CNT(rx_header);
			pr = PD_HEADER_PR(rx_header);
			dr = PD_HEADER_DR(rx_header);
			respect_state = NU_PD_STATE_NONE;
		}
	}

	g_rx_id = msg_id;

	return append_header;
}

static void nu6601_fsm_irq_handler(struct tcpc_device *tcpc)
{
	u8 status;
	u8 fsm;
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	/*NVT Vendor Defined INT reg*/
	status = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_INT);
	if(status <= 0)
		return;

	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, 0xff);

	if (status & INT_FSM_STA) {
		status = nu6601_i2c_read8(tcpc, NU6601_REG_CC_FSM_STATUS);
		g_fsm_status = status;
		if (status < 0) {
			nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, INT_FSM_STA);
			return ;
		}

		g_fsm_status = fsm = status & CC_FSM_STATUS_MASK;
		/*pr_err("CC FSM: %s triggered\n", nu6601_cc_fsm_status[fsm]);*/

		if (fsm != ATTACHWAIT_SNK) {
			cancel_delayed_work_sync(&chip->attach_wait_work);
			if (g_tcpc_nu6601->ver_id == NU6601_VER_A1)
				regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);
			else
				regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 1);
		}
		if (fsm == ATTACHED_SNK) {
			nu6601_pd_enable(tcpc, true);
			nu6601_pd_reset(tcpc);
			nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_STATUS_MASK, ~(UNATTACH_SNK_SRC_MASK | ATTACHED_SNK_SRC_MASK));
		} else if (fsm == ATTACHED_SRC) {
			nu6601_pd_enable(tcpc, true);
			nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_STATUS_MASK, ~UNATTACH_SNK_SRC_MASK);
		} else if ((fsm == UNATTACH_SNK) || (fsm == UNATTACH_SRC)) {
			nu6601_i2c_write8(chip->tcpc, NU6601_REG_CC_FSM_STATUS_MASK,
					~(ATTACHED_SNK_SRC_MASK | DEBUGACC_SNK_SRC_MASK | ATTACHWAIT_SNK_SRC_MASK));
			respect_state = NU_PD_STATE_NONE;
			g_rx_id = 0;
		} else if ((fsm == AUDIOACC) || (fsm == DEBUGACC_SNK)) {
			nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_STATUS_MASK, ~UNATTACH_SNK_SRC_MASK);
		} else if (fsm == ATTACHWAIT_SNK) {
			schedule_delayed_work(&chip->attach_wait_work, msecs_to_jiffies(200));
		}
	}

	return;
}

int nu6601_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask);
static void nu6601_irq_work_handler(struct kthread_work *work)
{
	struct nu6601_chip *chip =
			container_of(work, struct nu6601_chip, irq_work);
	int ret;
	int rv;
	uint8_t cnt;
	uint8_t buf[4];
	int regval = 0;
	int gpio_val;

	/* make sure I2C bus had resumed */
	down(&chip->suspend_lock);
	tcpci_lock_typec(chip->tcpc);

	do {
		ret = nu6601_i2c_read16(chip->tcpc, TCPC_V10_REG_ALERT);
		if (get_data_from_int) {
			int_alert = ret;
			if ((ret & 0x04) == 0x04) {
				rv = nu6601_block_read(chip->client,
						TCPC_V10_REG_RX_BYTE_CNT, 4, buf);

				if (int_msg_cnt != 0)
					pr_err("[crash] cnt:%x type:%x head:0x%x%x\n", buf[0], buf[1], buf[2], buf[3]);
				int_msg_cnt = cnt = buf[0];
				int_msg_type = buf[1];
				int_msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);

				if (cnt > 3) {
					cnt -= 3; /* MSG_HDR */
					rv = nu6601_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
							(uint8_t *) int_msg_data);
				}
				if ((int_alert & 0x01) == 0x01) {
					nu6601_alert_status_clear(chip->tcpc, TCPC_REG_ALERT_RX_MASK | 0x01);
					int_alert &= 0xfffe;
				} else {
					nu6601_alert_status_clear(chip->tcpc, TCPC_REG_ALERT_RX_MASK);
				}
			}

			if ((int_alert & 0x41) == 0x41)
				int_alert &= 0xfffE;
		}

		nu6601_fsm_irq_handler(chip->tcpc);

		/*pr_info("ret = 0x%x, g_fsm_status = %x\n",ret, g_fsm_status);*/
		if (ret == 0x80 || ret == 0x00 || ((ret == 0x81 || ret == 0x01) &&
					((g_fsm_status != AUDIOACC) &&
					 (g_fsm_status != DEBUGACC_SNK) &&
					 (g_fsm_status != ATTACHED_SRC) &&
					 (g_fsm_status != ATTACHED_SNK) &&
					 (g_fsm_status != UNATTACH_SNK) &&
					 (g_fsm_status != UNATTACH_SRC)))) {
			/*(g_fsm_status != UNATTACH_SRC) &&*/
			nu6601_i2c_write16(chip->tcpc, TCPC_V10_REG_ALERT, ret);
			//tcpci_unlock_typec(chip->tcpc);
			goto out;
		}

		regval = tcpci_alert(chip->tcpc);
		if (regval)
			break;
out:
		gpio_val = gpio_get_value(chip->irq_gpio);
	} while (gpio_val == 0);
	tcpci_unlock_typec(chip->tcpc);
	up(&chip->suspend_lock);
}

static irqreturn_t nu6601_intr_handler(int irq, void *data)
{
	struct nu6601_chip *chip = data;

	__pm_wakeup_event(chip->irq_wake_lock, NU6601_IRQ_WAKE_TIME);

	kthread_queue_work(&chip->irq_worker, &chip->irq_work);
	return IRQ_HANDLED;
}

static int nu6601_init_alert(struct tcpc_device *tcpc)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	int ret = 0;
	char *name = NULL;

	/* Clear Alert Mask & Status */
	nu6601_write_word(chip->client, TCPC_V10_REG_ALERT_MASK, 0);
	nu6601_write_word(chip->client, TCPC_V10_REG_ALERT, 0xffff);

	/* Claer Fsm Mask & Status*/
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_MASK, 0);
	nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, 0xff);

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

	kthread_init_worker(&chip->irq_worker);
	chip->irq_worker_task = kthread_run(kthread_worker_fn,
			&chip->irq_worker, "%s", chip->tcpc_desc->name);
	if (IS_ERR(chip->irq_worker_task)) {
		pr_err("Error: Could not create tcpc task\n");
		goto init_alert_err;
	}

	sched_setscheduler(chip->irq_worker_task, SCHED_FIFO, &param);
	kthread_init_work(&chip->irq_work, nu6601_irq_work_handler);

	pr_info("IRQF_NO_THREAD Test\n");

	ret = request_irq(chip->irq, nu6601_intr_handler,
			/*IRQF_TRIGGER_LOW | IRQF_ONESHOT, name, chip);*/
		IRQF_TRIGGER_FALLING | IRQF_NO_THREAD, name, chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request irq fail(%d)\n",
				__func__, ret);
		goto init_alert_err;
	}
	enable_irq_wake(chip->irq);

	return 0;

init_alert_err:
	return -EINVAL;
}

int nu6601_alert_status_clear(struct tcpc_device *tcpc, uint32_t mask)
{
	int ret;
	uint16_t mask_t1;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t mask_t2;
#endif
	/* Write 1 clear */
	mask_t1 = (uint16_t) mask;
	if (mask_t1) {
		ret = nu6601_i2c_write16(tcpc, TCPC_V10_REG_ALERT, mask_t1);
		if (ret < 0)
			return ret;
	}

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	mask_t2 = mask >> 16;
	if (mask_t2) {
		ret = nu6601_i2c_write8(tcpc, NU6601_REG_ANA_INT, mask_t2);
		if (ret < 0)
			return ret;
	}
#endif

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

int nu6601_fault_status_clear(struct tcpc_device *tcpc, uint8_t status)
{
	int ret;

	if (status & TCPC_V10_REG_FAULT_STATUS_VCONN_OV)
		ret = nu6601_fault_status_vconn_ov(tcpc);

	nu6601_i2c_write8(tcpc, TCPC_V10_REG_FAULT_STATUS, status);
	return 0;
}

int nu6601_get_alert_mask(struct tcpc_device *tcpc, uint32_t *mask)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = nu6601_i2c_read16(tcpc, TCPC_V10_REG_ALERT_MASK);
	if (ret < 0)
		return ret;

	*mask = (uint16_t) ret | TCPC_V10_REG_ALERT_CC_STATUS;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_MASK);
	if (ret < 0)
		return ret;

	v2 = (uint8_t) ret;
	*mask |= v2 << 16;
#endif

	return 0;
}

int nu6601_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	if (get_data_from_int) {
		*alert = int_alert;
	} else {
		ret = nu6601_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
		if (ret < 0)
			return ret;

		//pr_info("alert status %x\n", ret);
		*alert = (uint16_t) ret;
	}
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_INT);
	if (ret < 0)
		return ret;


	v2 = (uint8_t) ret;
	*alert |= v2 << 16;
#endif

	return 0;
}

int nu6601_rx_busy_get_alert_status(struct tcpc_device *tcpc, uint32_t *alert)
{
	int ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	uint8_t v2;
#endif

	ret = nu6601_i2c_read16(tcpc, TCPC_V10_REG_ALERT);
	if (ret < 0)
		return ret;
	pr_info("%s alert status %x\n", __func__, ret);
	*alert = (uint16_t) ret;
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_INT);
	if (ret < 0)
		return ret;
	v2 = (uint8_t) ret;
	*alert |= v2 << 16;
#endif
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
        /*if ((ret & TCPC_V10_REG_POWER_STATUS_VBUS_PRES) && ( */
                                /*g_fsm_status == ATTACHED_SNK || g_fsm_status == ATTACHED_SRC))*/
		*pwr_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;

#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
	ret = nu6601_i2c_read8(tcpc, NU6601_REG_ANA_STATUS);
	if (ret < 0)
		return ret;

	if (ret & VBUS_80)
		*pwr_status |= TCPC_REG_POWER_STATUS_EXT_VSAFE0V;
#endif
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

static int nu6601_get_cc(struct tcpc_device *tcpc, int *cc1, int *cc2)
{
	int status, role_ctrl, cc_role;
	bool act_as_sink, act_as_drp;

	status = nu6601_i2c_read8(tcpc, TCPC_V10_REG_CC_STATUS);
	if (status < 0)
		return status;

	role_ctrl = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
	if (role_ctrl < 0)
		return role_ctrl;

#if 0
	if (status & TCPC_V10_REG_CC_STATUS_DRP_TOGGLING) {
		*cc1 = TYPEC_CC_DRP_TOGGLING;
		*cc2 = TYPEC_CC_DRP_TOGGLING;
		return 0;
	}
#endif

	*cc1 = TCPC_V10_REG_CC_STATUS_CC1(status);
	*cc2 = TCPC_V10_REG_CC_STATUS_CC2(status);

	act_as_drp = TCPC_V10_REG_ROLE_CTRL_DRP & role_ctrl;

	if (act_as_drp) {
		act_as_sink = TCPC_V10_REG_CC_STATUS_DRP_RESULT(status);
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

static int nu6601_set_cc(struct tcpc_device *tcpc, int pull)
{
	int ret;
	uint8_t data;
	int rp_lvl = TYPEC_CC_PULL_GET_RP_LVL(pull), pull1, pull2;
/* N19A code for HQ-380111 by p-yanzelin at 20240423 start */
#if defined(CONFIG_PR_SWAP_FOR_NU6601)
	int pr_swap_pull = 0;
	uint8_t regval = 0x0;

	pr_swap_pull = pull;
	pr_info("%s : pr_swap_pull = %d\n", __func__, pr_swap_pull);
	if (tcpc->g_flag_role_swap == true) {
		if (pr_swap_pull == TYPEC_CC_OPEN) {
			regval = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
			regval = (regval & 0xF0) | 0x0f; //0x1AH : set CC_OPEN
			ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, regval);
		} else if (pr_swap_pull == TYPEC_CC_RP) {
			regval = nu6601_i2c_read8(tcpc, NU6601_REG_CC_FSM_CTRL);
			regval = (regval & 0x7f); //0xA6H : disable trysnk
			ret = nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_CTRL, regval);

			regval = nu6601_i2c_read8(tcpc, TCPC_V10_REG_ROLE_CTRL);
			regval = (regval & 0xB0) | 0x05; //0x1AH:set CC_RP and no DRP
			ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, regval);
		} else if (pr_swap_pull == TYPEC_CC_DRP) {
			regval = nu6601_i2c_read8(tcpc, NU6601_REG_CC_FSM_CTRL);
			regval = (regval | 0x80); //0xA6H : enable trysnk
			ret = nu6601_i2c_write8(tcpc, NU6601_REG_CC_FSM_CTRL, regval);
		} else if ((pr_swap_pull == TYPEC_CC_RP_3_0 || pr_swap_pull == TYPEC_CC_RP_1_5)
			&& tcpc->typec_is_attached_src) {
			data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, 0, 0);
			ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, (data & 0xf0) | 0x05);
		}
		return 0;
	}
#endif
/* N19A code for HQ-380111 by p-yanzelin at 20240423 end */

	NU6601_INFO("\n");
	pull = TYPEC_CC_PULL_GET_RES(pull);
	if (pull == TYPEC_CC_DRP) {
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(
				1, rp_lvl, TYPEC_CC_RD, TYPEC_CC_RD);

		ret = nu6601_i2c_write8(
				tcpc, TCPC_V10_REG_ROLE_CTRL, (data & 0xf0) | 0x0a);

		if (ret == 0) {
#ifdef CONFIG_TCPC_VSAFE0V_DETECT_IC
			nu6601_enable_vsafe0v_detect(tcpc, false);
#endif /* CONFIG_TCPC_VSAFE0V_DETECT_IC */
			ret = nu6601_command(tcpc, TCPM_CMD_LOOK_CONNECTION);
		}
	} else {
#if IS_ENABLED(CONFIG_USB_POWER_DELIVERY)
		if (pull == TYPEC_CC_RD && tcpc->pd_wait_pr_swap_complete)
			nu6601_init_cc_params(tcpc, TYPEC_CC_VOLT_SNK_DFT);
#endif	/* CONFIG_USB_POWER_DELIVERY */

		pull1 = pull2 = pull;

		if ((pull == TYPEC_CC_RP_DFT || pull == TYPEC_CC_RP_1_5 ||
			pull == TYPEC_CC_RP_3_0) &&
			tcpc->typec_is_attached_src) {
			if (tcpc->typec_polarity)
				pull1 = TYPEC_CC_OPEN;
			else
				pull2 = TYPEC_CC_OPEN;
		}
		data = TCPC_V10_REG_ROLE_CTRL_RES_SET(0, rp_lvl, pull1, pull2);
		ret = nu6601_i2c_write8(tcpc, TCPC_V10_REG_ROLE_CTRL, (data & 0xf0) | 0x0a);
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

	return 0;
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
	if (g_tcpc_nu6601->ver_id == NU6601_VER_A1)
		regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 1);
	else
		regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);
#else
	nu6601_i2c_write8(tcpc_dev, NU6601_REG_SWRESET, 1);
#endif	/* CONFIG_TCPC_SHUTDOWN_CC_DETACH */
	return 0;
}

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


static int nu6601_get_message(struct tcpc_device *tcpc, uint32_t *payload,
			uint16_t *msg_head, enum tcpm_transmit_type *frame_type)
{
	uint16_t header;

	if (get_data_from_int) {
		if (int_msg_cnt > 0) {
			*frame_type = int_msg_type;
			if (apend_ps_rdy) {
				header = nu6601_append_data(int_msg_head);
				if (!header)
					*msg_head = int_msg_head;
				else
					*msg_head = header;
			} else {
				*msg_head = int_msg_head;
			}

			if (int_msg_cnt > 3)
				memcpy((uint8_t *)payload, int_msg_data, int_msg_cnt - 3);

			int_msg_cnt = 0;
		}
		return 0;
	} else {
		struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);
		int rv;
		uint8_t cnt = 0;
		uint8_t buf[4];
		const uint16_t alert_rx =
			TCPC_V10_REG_ALERT_RX_STATUS|TCPC_V10_REG_RX_OVERFLOW;

		rv = nu6601_block_read(chip->client,
				TCPC_V10_REG_RX_BYTE_CNT, 4, buf);

		if (rv < 0)
			return rv;

		cnt = buf[0];
		*frame_type = buf[1];

		if (apend_ps_rdy) {
			header = nu6601_append_data(le16_to_cpu(*(uint16_t *)&buf[2]));
			if (!header) {
				*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);
			 } else {
				*msg_head = header;
				pr_err("new header: 0x%x\n", *msg_head);
			 }
		} else {
			*msg_head = le16_to_cpu(*(uint16_t *)&buf[2]);
		}

		/* TCPC 1.0 ==> no need to subtract the size of msg_head */
		if (cnt > 3) {
			cnt -= 3; /* MSG_HDR */
			rv = nu6601_block_read(chip->client, TCPC_V10_REG_RX_DATA, cnt,
					(uint8_t *) payload);
		}

		//pr_info("cnt=%d,type=%d,head=%x\n", cnt,*frame_type,*msg_head);


		/* Read complete, clear RX status alert bit */
		nu6601_alert_status_clear(tcpc, alert_rx);

		return rv;
	}
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
	struct tcpc_transmit_packet packet;

	if (apend_ps_rdy)
		nu6601_trans_machine(header);

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

	rv = nu6601_i2c_write8(tcpc, TCPC_V10_REG_TRANSMIT,
			TCPC_V10_REG_TRANSMIT_SET(
			tcpc->pd_retry_count, type));
	return rv;
}

static int nu6601_set_bist_test_mode(struct tcpc_device *tcpc, bool en)
{
	int data;

	data = nu6601_i2c_read8(tcpc, TCPC_V10_REG_TCPC_CTRL);
	if (data < 0)
		return data;

	data &= ~TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE;
	data |= en ? TCPC_V10_REG_TCPC_CTRL_BIST_TEST_MODE : 0;

	return nu6601_i2c_write8(tcpc, TCPC_V10_REG_TCPC_CTRL, data);
}
#endif /* CONFIG_USB_POWER_DELIVERY */

static int nu6601_get_chip_id(struct tcpc_device *tcpc, uint32_t *chip_id)
{
	struct nu6601_chip *chip = tcpc_get_dev_data(tcpc);

	if (chip->chip_id == 0)
		*chip_id = 0x6601;

	return 0;
}


static struct tcpc_ops nu6601_tcpc_ops = {
	.init = nu6601_tcpc_init,
	.alert_status_clear = nu6601_alert_status_clear,
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
	.rx_busy_get_alert_status = nu6601_rx_busy_get_alert_status,
	.get_chip_id = nu6601_get_chip_id,
};

static int nu6601_parse_dt(struct nu6601_chip *chip, struct device *dev)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = of_get_named_gpio(np, "rt1711pd,intr_gpio", 0);
	if (ret < 0) {
		pr_err("%s no intr_gpio info\n", __func__);
		return ret;
	}
	chip->irq_gpio = ret;

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

#define NU6601_VID	0x00
#define NU6601_PID	0x00

static inline int nu6601_check_revision(struct i2c_client *client)
{
	u16 vid, pid, did;
	int ret;

	ret = nu6601_read_device(client, TCPC_V10_REG_VID, 2, &vid);
	if (ret < 0) {
		dev_err(&client->dev, "read chip ID fail\n");
		return -EIO;
	}

	if (vid != NU6601_VID) {
		pr_info("%s failed, VID=0x%04x\n", __func__, vid);
		return -ENODEV;
	}

	ret = nu6601_read_device(client, TCPC_V10_REG_PID, 2, &pid);
	if (ret < 0) {
		dev_err(&client->dev, "read product ID fail\n");
		return -EIO;
	}

	if (pid != NU6601_PID) {
		pr_info("%s failed, PID=0x%04x\n", __func__, pid);
		return -ENODEV;
	}

	ret = nu6601_read_device(client, TCPC_V10_REG_DID, 2, &did);
	if (ret < 0) {
		dev_err(&client->dev, "read device ID fail\n");
		return -EIO;
	}

	return did;
}

static void nu6601_attach_wait_func(struct work_struct *work)
{
	struct nu6601_chip *chip = container_of(work, struct nu6601_chip, attach_wait_work.work);
	u8 status;
	status = nu6601_i2c_read8(chip->tcpc, NU6601_REG_CC_FSM_STATUS);
	if ((status & CC_FSM_STATUS_MASK) == ATTACHWAIT_SNK) {
		if (g_tcpc_nu6601->ver_id == NU6601_VER_A1)
			regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 1);
		else
			regmap_update_bits(chip->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);
	}
}

static int nu6601_tcpc_notify_cb(struct notifier_block *nb,
		unsigned long event, void *data)
{
	//struct nu6601_evt_data *nu6601_data = data;
	uint8_t val = *((uint8_t *)data);
	dev_err(&g_tcpc_nu6601->client->dev, "event :  %ld, val %d\n", event, val);

	switch(event) {
		case NU6601_EVENT_DATA_INT:
			get_data_from_int = val;
			break;
		case NU6601_EVENT_APEND_DATA:
			apend_ps_rdy = val;
			break;
		case NU6601_EVENT_CC:
			if (val == 1) {
				tcpm_typec_change_role(g_tcpc_nu6601->tcpc, TYPEC_ROLE_DRP);
				pr_info("cid detect plugged in");
			} else {
				tcpm_typec_change_role(g_tcpc_nu6601->tcpc, TYPEC_ROLE_SNK);
				nu6601_reset_cc(g_tcpc_nu6601->tcpc);
				nu6601_pd_enable(g_tcpc_nu6601->tcpc, false);
				pr_info("cid detect plugged out");
/* N19A code for HQ-380111 by p-yanzelin at 20240423 start */
#if defined(CONFIG_PR_SWAP_FOR_NU6601)
				g_tcpc_nu6601->tcpc->g_flag_role_swap = false;
#endif
/* N19A code for HQ-380111 by p-yanzelin at 20240423 end */
			}
			break;
		case NU6601_EVENT_VER:
			g_tcpc_nu6601->ver_id = val;
			if (g_tcpc_nu6601->ver_id == NU6601_VER_A1) {
				regmap_update_bits(g_tcpc_nu6601->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);
				nu6601_set_cc(g_tcpc_nu6601->tcpc, TYPEC_CC_DRP);
			}
		case 8:
			regmap_update_bits(g_tcpc_nu6601->regmap, NU6601_REG_ANA_CTRL1, DEAD_BAT_FORCEOFF, 0);
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
	chip->irq_wake_lock =
		wakeup_source_register(chip->dev, "nu6601_irq_wake_lock");

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

	INIT_DELAYED_WORK(&chip->attach_wait_work, nu6601_attach_wait_func);

	tcpc_schedule_init_work(chip->tcpc);
	g_tcpc_nu6601 = chip;
	pr_err("%s probe OK!\n", __func__);
	return 0;

err_irq_init:
	tcpc_device_unregister(chip->dev, chip->tcpc);
err_tcpc_reg:
	nu6601_regmap_deinit(chip);
err_regmap_init:
	wakeup_source_unregister(chip->irq_wake_lock);
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
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			down(&chip->suspend_lock);
	}

	return 0;
}

static int nu6601_i2c_resume(struct device *dev)
{
	struct nu6601_chip *chip = dev_get_drvdata(dev);
	struct i2c_client *client = to_i2c_client(dev);

	if (client) {
		chip = i2c_get_clientdata(client);
		if (chip)
			up(&chip->suspend_lock);
	}
	dev_info(dev, "%s\n", __func__);

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
MODULE_AUTHOR("Jeff Chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("NU6601 TCPC Driver");
MODULE_VERSION(NU6601_DRV_VERSION);

/**** Release Note ****
 * 1.0.1_NVT
 * NVT first released PD3.0 Driver
 */

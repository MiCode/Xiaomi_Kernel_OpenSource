/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spmi.h>
#include <linux/workqueue.h>
#include <linux/bif/driver.h>
#include <linux/qpnp/qpnp-adc.h>

enum qpnp_bsi_irq {
	QPNP_BSI_IRQ_ERR,
	QPNP_BSI_IRQ_RX,
	QPNP_BSI_IRQ_TX,
	QPNP_BSI_IRQ_COUNT,
};

enum qpnp_bsi_com_mode {
	QPNP_BSI_COM_MODE_IRQ,
	QPNP_BSI_COM_MODE_POLL,
};

struct qpnp_bsi_chip {
	struct bif_ctrl_desc	bdesc;
	struct spmi_device	*spmi_dev;
	struct bif_ctrl_dev	*bdev;
	struct work_struct	slave_irq_work;
	u16			base_addr;
	u16			batt_id_stat_addr;
	int			r_pullup_ohm;
	int			vid_ref_uV;
	int			tau_index;
	int			tau_sampling_mask;
	enum bif_bus_state	state;
	enum qpnp_bsi_com_mode	com_mode;
	int			irq[QPNP_BSI_IRQ_COUNT];
	atomic_t		irq_flag[QPNP_BSI_IRQ_COUNT];
	int			batt_present_irq;
	enum qpnp_vadc_channels	batt_id_adc_channel;
	struct qpnp_vadc_chip	*vadc_dev;
};

#define QPNP_BSI_DRIVER_NAME	"qcom,qpnp-bsi"

enum qpnp_bsi_registers {
	QPNP_BSI_REG_TYPE		= 0x04,
	QPNP_BSI_REG_SUBTYPE		= 0x05,
	QPNP_BSI_REG_STATUS		= 0x08,
	QPNP_BSI_REG_ENABLE		= 0x46,
	QPNP_BSI_REG_CLEAR_ERROR	= 0x4F,
	QPNP_BSI_REG_FORCE_BCL_LOW	= 0x51,
	QPNP_BSI_REG_TAU_CONFIG		= 0x52,
	QPNP_BSI_REG_MODE		= 0x53,
	QPNP_BSI_REG_RX_TX_ENABLE	= 0x54,
	QPNP_BSI_REG_TX_DATA_LOW	= 0x5A,
	QPNP_BSI_REG_TX_DATA_HIGH	= 0x5B,
	QPNP_BSI_REG_TX_CTRL		= 0x5D,
	QPNP_BSI_REG_RX_DATA_LOW	= 0x60,
	QPNP_BSI_REG_RX_DATA_HIGH	= 0x61,
	QPNP_BSI_REG_RX_SOURCE		= 0x62,
	QPNP_BSI_REG_BSI_ERROR		= 0x70,
};

#define QPNP_BSI_TYPE			0x02
#define QPNP_BSI_SUBTYPE		0x10

#define QPNP_BSI_STATUS_ERROR		0x10
#define QPNP_BSI_STATUS_TX_BUSY		0x08
#define QPNP_BSI_STATUS_RX_BUSY		0x04
#define QPNP_BSI_STATUS_TX_GO_BUSY	0x02
#define QPNP_BSI_STATUS_RX_DATA_READY	0x01

#define QPNP_BSI_ENABLE_MASK		0x80
#define QPNP_BSI_ENABLE			0x80
#define QPNP_BSI_DISABLE		0x00

#define QPNP_BSI_TAU_CONFIG_SAMPLE_MASK	0x10
#define QPNP_BSI_TAU_CONFIG_SAMPLE_8X	0x10
#define QPNP_BSI_TAU_CONFIG_SAMPLE_4X	0x00
#define QPNP_BSI_TAU_CONFIG_SPEED_MASK	0x07

#define QPNP_BSI_MODE_TX_PULSE_MASK	0x10
#define QPNP_BSI_MODE_TX_PULSE_INT	0x10
#define QPNP_BSI_MODE_TX_PULSE_DATA	0x00
#define QPNP_BSI_MODE_RX_PULSE_MASK	0x08
#define QPNP_BSI_MODE_RX_PULSE_INT	0x08
#define QPNP_BSI_MODE_RX_PULSE_DATA	0x00
#define QPNP_BSI_MODE_TX_PULSE_T_MASK	0x04
#define QPNP_BSI_MODE_TX_PULSE_T_WAKE	0x04
#define QPNP_BSI_MODE_TX_PULSE_T_1_TAU	0x00
#define QPNP_BSI_MODE_RX_FORMAT_MASK	0x02
#define QPNP_BSI_MODE_RX_FORMAT_17_BIT	0x02
#define QPNP_BSI_MODE_RX_FORMAT_11_BIT	0x00
#define QPNP_BSI_MODE_TX_FORMAT_MASK	0x01
#define QPNP_BSI_MODE_TX_FORMAT_17_BIT	0x01
#define QPNP_BSI_MODE_TX_FORMAT_11_BIT	0x00

#define QPNP_BSI_TX_ENABLE_MASK		0x80
#define QPNP_BSI_TX_ENABLE		0x80
#define QPNP_BSI_TX_DISABLE		0x00
#define QPNP_BSI_RX_ENABLE_MASK		0x40
#define QPNP_BSI_RX_ENABLE		0x40
#define QPNP_BSI_RX_DISABLE		0x00

#define QPNP_BSI_TX_DATA_HIGH_MASK	0x07

#define QPNP_BSI_TX_CTRL_GO		0x01

#define QPNP_BSI_RX_DATA_HIGH_MASK	0x07

#define QPNP_BSI_RX_SRC_LOOPBACK_FLAG	0x10

#define QPNP_BSI_BSI_ERROR_CLEAR	0x80

#define QPNP_SMBB_BAT_IF_BATT_PRES_MASK	0x80
#define QPNP_SMBB_BAT_IF_BATT_ID_MASK	0x01

#define QPNP_BSI_NUM_CLOCK_PERIODS	8

struct qpnp_bsi_tau {
	int period_4x_ns[QPNP_BSI_NUM_CLOCK_PERIODS];
	int period_8x_ns[QPNP_BSI_NUM_CLOCK_PERIODS];
	int period_4x_us[QPNP_BSI_NUM_CLOCK_PERIODS];
	int period_8x_us[QPNP_BSI_NUM_CLOCK_PERIODS];
};

/* Tau BIF clock periods in ns supported by BSI for either 4x or 8x sampling. */
static const struct qpnp_bsi_tau qpnp_bsi_tau_period = {
	.period_4x_ns = {
		150420, 122080, 61040, 31670, 15830, 7920, 3960, 2080
	},
	.period_8x_ns = {
		150420, 122080, 63330, 31670, 15830, 7920, 4170, 2080
	},
	.period_4x_us = {
		151, 122, 61, 32, 16, 8, 4, 2
	},
	.period_8x_us = {
		151, 122, 64, 32, 16, 8, 4, 2
	},

};
#define QPNP_BSI_MIN_CLOCK_SPEED_NS	2080
#define QPNP_BSI_MAX_CLOCK_SPEED_NS	150420

#define QPNP_BSI_MIN_PULLUP_OHM		1000
#define QPNP_BSI_MAX_PULLUP_OHM		500000
#define QPNP_BSI_DEFAULT_PULLUP_OHM	100000
#define QPNP_BSI_MIN_VID_REF_UV		500000
#define QPNP_BSI_MAX_VID_REF_UV		5000000
#define QPNP_BSI_DEFAULT_VID_REF_UV	1800000

/* These have units of tau_bif. */
#define QPNP_BSI_MAX_TRANSMIT_CYCLES	46
#define QPNP_BSI_MIN_RECEIVE_CYCLES	24
#define QPNP_BSI_MAX_BUS_QUERY_CYCLES	17

/*
 * Maximum time in microseconds for a slave to transition from suspend to active
 * state.
 */
#define QPNP_BSI_MAX_SLAVE_ACTIVIATION_DELAY_US	50

/*
 * Maximum time in milliseconds for a slave to transition from power down to
 * active state.
 */
#define QPNP_BSI_MAX_SLAVE_POWER_UP_DELAY_MS	10

#define QPNP_BSI_POWER_UP_LOW_DELAY_US		240

/*
 * Latencies that are used when determining if polling or interrupts should be
 * used for a given transaction.
 */
#define QPNP_BSI_MAX_IRQ_LATENCY_US		170
#define QPNP_BSI_MAX_BSI_DATA_READ_LATENCY_US	16

static int qpnp_bsi_set_bus_state(struct bif_ctrl_dev *bdev, int state);

static inline int qpnp_bsi_read(struct qpnp_bsi_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_readl() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}

static inline int qpnp_bsi_write(struct qpnp_bsi_chip *chip, u16 addr, u8 *buf,
				int len)
{
	int rc;

	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl,
			chip->spmi_dev->sid, chip->base_addr + addr, buf, len);

	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_writel() failed. sid=%d, addr=%04X, len=%d, rc=%d\n",
			__func__, chip->spmi_dev->sid, chip->base_addr + addr,
			len, rc);

	return rc;
}

enum qpnp_bsi_rx_tx_state {
	QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF,
	QPNP_BSI_RX_TX_STATE_RX_OFF_TX_DATA,
	QPNP_BSI_RX_TX_STATE_RX_OFF_TX_INT,
	QPNP_BSI_RX_TX_STATE_RX_INT_TX_DATA,
	QPNP_BSI_RX_TX_STATE_RX_DATA_TX_DATA,
	QPNP_BSI_RX_TX_STATE_RX_INT_TX_OFF,
};

static int qpnp_bsi_rx_tx_config(struct qpnp_bsi_chip *chip,
				    enum qpnp_bsi_rx_tx_state state)
{
	u8 buf[2] = {0, 0};
	int rc;

	buf[0] = QPNP_BSI_MODE_TX_FORMAT_11_BIT
		 | QPNP_BSI_MODE_RX_FORMAT_11_BIT;

	switch (state) {
	case QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_DATA |
			QPNP_BSI_MODE_RX_PULSE_DATA;
		buf[1] = QPNP_BSI_TX_DISABLE | QPNP_BSI_RX_DISABLE;
		break;
	case QPNP_BSI_RX_TX_STATE_RX_OFF_TX_DATA:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_DATA |
			QPNP_BSI_MODE_RX_PULSE_DATA;
		buf[1] = QPNP_BSI_TX_ENABLE | QPNP_BSI_RX_DISABLE;
		break;
	case QPNP_BSI_RX_TX_STATE_RX_OFF_TX_INT:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_INT |
			QPNP_BSI_MODE_RX_PULSE_DATA;
		buf[1] = QPNP_BSI_TX_ENABLE | QPNP_BSI_RX_DISABLE;
		break;
	case QPNP_BSI_RX_TX_STATE_RX_INT_TX_DATA:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_DATA |
			QPNP_BSI_MODE_RX_PULSE_INT;
		buf[1] = QPNP_BSI_TX_ENABLE | QPNP_BSI_RX_ENABLE;
		break;
	case QPNP_BSI_RX_TX_STATE_RX_DATA_TX_DATA:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_DATA |
			QPNP_BSI_MODE_RX_PULSE_DATA;
		buf[1] = QPNP_BSI_TX_ENABLE | QPNP_BSI_RX_ENABLE;
		break;
	case QPNP_BSI_RX_TX_STATE_RX_INT_TX_OFF:
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_DATA |
			QPNP_BSI_MODE_RX_PULSE_INT;
		buf[1] = QPNP_BSI_TX_DISABLE | QPNP_BSI_RX_DISABLE;
		break;
	default:
		dev_err(&chip->spmi_dev->dev, "%s: invalid state=%d\n",
			__func__, state);
		return -EINVAL;
	}

	rc = qpnp_bsi_write(chip, QPNP_BSI_REG_MODE, buf, 2);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
			__func__, rc);

	return rc;
}

static void qpnp_bsi_slave_irq_work(struct work_struct *work)
{
	struct qpnp_bsi_chip *chip
		= container_of(work, struct qpnp_bsi_chip, slave_irq_work);
	int rc;

	rc = bif_ctrl_notify_slave_irq(chip->bdev);
	if (rc)
		pr_err("Could not notify BIF core about slave interrupt, rc=%d\n",
			rc);
}

static irqreturn_t qpnp_bsi_isr(int irq, void *data)
{
	struct qpnp_bsi_chip *chip = data;
	bool found = false;
	int i;

	for (i = 0; i < QPNP_BSI_IRQ_COUNT; i++) {
		if (irq == chip->irq[i]) {
			found = true;
			atomic_cmpxchg(&chip->irq_flag[i], 0, 1);

			/* Check if this is a slave interrupt. */
			if (i == QPNP_BSI_IRQ_RX
			    && chip->state == BIF_BUS_STATE_INTERRUPT) {
				/* Slave IRQ makes the bus active. */
				qpnp_bsi_rx_tx_config(chip,
					QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);
				chip->state = BIF_BUS_STATE_ACTIVE;
				schedule_work(&chip->slave_irq_work);
			}
		}
	}

	if (!found)
		pr_err("Unknown interrupt: %d\n", irq);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_bsi_batt_present_isr(int irq, void *data)
{
	struct qpnp_bsi_chip *chip = data;
	int rc;

	if (!chip->bdev)
		return IRQ_HANDLED;

	rc = bif_ctrl_notify_battery_changed(chip->bdev);
	if (rc)
		pr_err("Could not notify about battery state change, rc=%d\n",
			rc);

	return IRQ_HANDLED;
}

static void qpnp_bsi_set_com_mode(struct qpnp_bsi_chip *chip,
				enum qpnp_bsi_com_mode mode)
{
	int i;

	if (chip->com_mode == mode)
		return;

	if (mode == QPNP_BSI_COM_MODE_IRQ)
		for (i = 0; i < QPNP_BSI_IRQ_COUNT; i++)
			enable_irq(chip->irq[i]);
	else
		for (i = 0; i < QPNP_BSI_IRQ_COUNT; i++)
			disable_irq(chip->irq[i]);

	chip->com_mode = mode;
}

static inline bool qpnp_bsi_check_irq(struct qpnp_bsi_chip *chip, int irq)
{
	return atomic_cmpxchg(&chip->irq_flag[irq], 1, 0);
}

static void qpnp_bsi_clear_irq_flags(struct qpnp_bsi_chip *chip)
{
	int i;

	for (i = 0; i < QPNP_BSI_IRQ_COUNT; i++)
		atomic_set(&chip->irq_flag[i], 0);
}

static inline int qpnp_bsi_get_tau_ns(struct qpnp_bsi_chip *chip)
{
	if (chip->tau_sampling_mask == QPNP_BSI_TAU_CONFIG_SAMPLE_4X)
		return qpnp_bsi_tau_period.period_4x_ns[chip->tau_index];
	else
		return qpnp_bsi_tau_period.period_8x_ns[chip->tau_index];
}

static inline int qpnp_bsi_get_tau_us(struct qpnp_bsi_chip *chip)
{
	if (chip->tau_sampling_mask == QPNP_BSI_TAU_CONFIG_SAMPLE_4X)
		return qpnp_bsi_tau_period.period_4x_us[chip->tau_index];
	else
		return qpnp_bsi_tau_period.period_8x_us[chip->tau_index];
}

/* Checks if BSI is in an error state and clears the error if it is. */
static int qpnp_bsi_clear_bsi_error(struct qpnp_bsi_chip *chip)
{
	int rc, delay_us;
	u8 reg;

	rc = qpnp_bsi_read(chip, QPNP_BSI_REG_BSI_ERROR, &reg, 1);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_read() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (reg > 0) {
		/*
		 * Delay before clearing the BSI error in case a transaction is
		 * still in flight.
		 */
		delay_us = QPNP_BSI_MAX_TRANSMIT_CYCLES
				* qpnp_bsi_get_tau_us(chip);
		udelay(delay_us);

		pr_info("PMIC BSI module in error state, error=%d\n", reg);

		reg = QPNP_BSI_BSI_ERROR_CLEAR;
		rc = qpnp_bsi_write(chip, QPNP_BSI_REG_CLEAR_ERROR, &reg, 1);
		if (rc)
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
	}

	return rc;
}

static int qpnp_bsi_get_bsi_error(struct qpnp_bsi_chip *chip)
{
	int rc;
	u8 reg;

	rc = qpnp_bsi_read(chip, QPNP_BSI_REG_BSI_ERROR, &reg, 1);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_read() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return reg;
}

static int qpnp_bsi_wait_for_tx(struct qpnp_bsi_chip *chip, int timeout)
{
	int rc = 0;

	/* Wait for TX or ERR IRQ. */
	while (timeout > 0) {
		if (qpnp_bsi_check_irq(chip, QPNP_BSI_IRQ_ERR)) {
			dev_err(&chip->spmi_dev->dev, "%s: transaction error occurred, BSI error=%d\n",
				__func__, qpnp_bsi_get_bsi_error(chip));
			return -EIO;
		}

		if (qpnp_bsi_check_irq(chip, QPNP_BSI_IRQ_TX))
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0) {
		rc = -ETIMEDOUT;
		dev_err(&chip->spmi_dev->dev, "%s: transaction timed out, no interrupts received, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return rc;
}

static int qpnp_bsi_issue_transaction(struct qpnp_bsi_chip *chip,
		int transaction, u8 data)
{
	int rc;
	u8 buf[4];

	/* MIPI_BIF_DATA_TX_0 = BIF word bits 7 to 0 */
	buf[0] = data;
	/* MIPI_BIF_DATA_TX_1 = BIF word BCF, bits 9 to 8 */
	buf[1] = transaction & QPNP_BSI_TX_DATA_HIGH_MASK;
	/* MIPI_BIF_DATA_TX_2 ignored */
	buf[2] = 0x00;
	/* MIPI_BIF_TX_CTL bit 0 written to start the transaction. */
	buf[3] = QPNP_BSI_TX_CTRL_GO;

	/* Write the TX_DATA bytes and initiate the transaction. */
	rc = qpnp_bsi_write(chip, QPNP_BSI_REG_TX_DATA_LOW, buf, 4);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
			__func__, rc);
	return rc;
}

static int qpnp_bsi_issue_transaction_wait_for_tx(struct qpnp_bsi_chip *chip,
		int transaction, u8 data)
{
	int rc, timeout;

	rc = qpnp_bsi_issue_transaction(chip, transaction, data);
	if (rc)
		return rc;

	timeout = QPNP_BSI_MAX_TRANSMIT_CYCLES * qpnp_bsi_get_tau_us(chip)
			+ QPNP_BSI_MAX_IRQ_LATENCY_US;

	rc = qpnp_bsi_wait_for_tx(chip, timeout);

	return rc;
}

static int qpnp_bsi_wait_for_rx(struct qpnp_bsi_chip *chip, int timeout)
{
	int rc = 0;

	/* Wait for RX IRQ to indicate that data is ready to read. */
	while (timeout > 0) {
		if (qpnp_bsi_check_irq(chip, QPNP_BSI_IRQ_ERR)) {
			dev_err(&chip->spmi_dev->dev, "%s: transaction error occurred, BSI error=%d\n",
				__func__, qpnp_bsi_get_bsi_error(chip));
			return -EIO;
		}

		if (qpnp_bsi_check_irq(chip, QPNP_BSI_IRQ_RX))
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0)
		rc = -ETIMEDOUT;

	return rc;
}

static int qpnp_bsi_bus_transaction(struct bif_ctrl_dev *bdev, int transaction,
				u8 data)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc;

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_IRQ);

	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_DATA);
	if (rc)
		return rc;

	rc = qpnp_bsi_issue_transaction_wait_for_tx(chip, transaction, data);
	if (rc)
		return rc;

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);

	return rc;
}

static int qpnp_bsi_bus_transaction_query(struct bif_ctrl_dev *bdev,
				int transaction, u8 data, bool *query_response)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc, timeout;

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_IRQ);

	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_INT_TX_DATA);
	if (rc)
		return rc;

	rc = qpnp_bsi_issue_transaction_wait_for_tx(chip, transaction, data);
	if (rc)
		return rc;

	timeout = QPNP_BSI_MAX_BUS_QUERY_CYCLES * qpnp_bsi_get_tau_us(chip)
				+ QPNP_BSI_MAX_IRQ_LATENCY_US;

	rc = qpnp_bsi_wait_for_rx(chip, timeout);
	if (rc == 0) {
		*query_response = true;
	} else if (rc == -ETIMEDOUT) {
		*query_response = false;
		rc = 0;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);

	return rc;
}

static int qpnp_bsi_bus_transaction_read(struct bif_ctrl_dev *bdev,
				int transaction, u8 data, int *response)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc, timeout;
	u8 buf[3];

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_IRQ);

	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_DATA_TX_DATA);
	if (rc)
		return rc;

	rc = qpnp_bsi_issue_transaction_wait_for_tx(chip, transaction, data);
	if (rc)
		return rc;

	timeout = QPNP_BSI_MAX_TRANSMIT_CYCLES * qpnp_bsi_get_tau_us(chip)
				+ QPNP_BSI_MAX_IRQ_LATENCY_US;

	rc = qpnp_bsi_wait_for_rx(chip, timeout);
	if (rc) {
		if (rc == -ETIMEDOUT) {
			/*
			 * No error message is printed in this case in order
			 * to provide silent operation when checking if a slave
			 * is selected using the transaction query bus command.
			 */
			dev_dbg(&chip->spmi_dev->dev, "%s: transaction timed out, no interrupts received, rc=%d\n",
					__func__, rc);
		}
		return rc;
	}

	/* Read the RX_DATA bytes. */
	rc = qpnp_bsi_read(chip, QPNP_BSI_REG_RX_DATA_LOW, buf, 3);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_read() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (buf[2] & QPNP_BSI_RX_SRC_LOOPBACK_FLAG) {
		rc = -EIO;
		dev_err(&chip->spmi_dev->dev, "%s: unexpected loopback data read, rc=%d\n",
			__func__, rc);
		return rc;
	}

	*response = ((int)(buf[1] & QPNP_BSI_RX_DATA_HIGH_MASK) << 8) | buf[0];

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);

	return 0;
}

/*
 * Wait for RX_FLOW_STATUS to be set to 1 which indicates that another BIF word
 * can be read from PMIC registers.
 */
static int qpnp_bsi_wait_for_rx_data(struct qpnp_bsi_chip *chip)
{
	int rc = 0;
	int timeout;
	u8 reg;

	timeout = QPNP_BSI_MAX_TRANSMIT_CYCLES * qpnp_bsi_get_tau_us(chip);

	/* Wait for RX_FLOW_STATUS == 1 or ERR_FLAG == 1. */
	while (timeout > 0) {
		rc = qpnp_bsi_read(chip, QPNP_BSI_REG_STATUS, &reg, 1);
		if (rc) {
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
			return rc;
		}

		if (reg & QPNP_BSI_STATUS_ERROR) {
			dev_err(&chip->spmi_dev->dev, "%s: transaction error occurred, BSI error=%d\n",
				__func__, qpnp_bsi_get_bsi_error(chip));
			return -EIO;
		}

		if (reg & QPNP_BSI_STATUS_RX_DATA_READY) {
			/* BSI RX has data word latched. */
			return 0;
		}

		udelay(1);
		timeout--;
	}

	rc = -ETIMEDOUT;
	dev_err(&chip->spmi_dev->dev, "%s: transaction timed out, RX_FLOW_STATUS never set to 1, rc=%d\n",
		__func__, rc);

	return rc;
}

/*
 * Wait for TX_GO_STATUS to be set to 0 which indicates that another BIF word
 * can be enqueued.
 */
static int qpnp_bsi_wait_for_tx_go(struct qpnp_bsi_chip *chip)
{
	int rc = 0;
	int timeout;
	u8 reg;

	timeout = QPNP_BSI_MAX_TRANSMIT_CYCLES * qpnp_bsi_get_tau_us(chip);

	/* Wait for TX_GO_STATUS == 0 or ERR_FLAG == 1. */
	while (timeout > 0) {
		rc = qpnp_bsi_read(chip, QPNP_BSI_REG_STATUS, &reg, 1);
		if (rc) {
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
			return rc;
		}

		if (reg & QPNP_BSI_STATUS_ERROR) {
			dev_err(&chip->spmi_dev->dev, "%s: transaction error occurred, BSI error=%d\n",
				__func__, qpnp_bsi_get_bsi_error(chip));
			return -EIO;
		}

		if (!(reg & QPNP_BSI_STATUS_TX_GO_BUSY)) {
			/* BSI TX is ready to accept the next word. */
			return 0;
		}

		udelay(1);
		timeout--;
	}

	rc = -ETIMEDOUT;
	dev_err(&chip->spmi_dev->dev, "%s: transaction timed out, TX_GO_STATUS never set to 0, rc=%d\n",
		__func__, rc);

	return rc;
}

/*
 * Wait for TX_BUSY to be set to 0 which indicates that the TX data has been
 * successfully transmitted.
 */
static int qpnp_bsi_wait_for_tx_idle(struct qpnp_bsi_chip *chip)
{
	int rc = 0;
	int timeout;
	u8 reg;

	timeout = QPNP_BSI_MAX_TRANSMIT_CYCLES * qpnp_bsi_get_tau_us(chip);

	/* Wait for TX_BUSY == 0 or ERR_FLAG == 1. */
	while (timeout > 0) {
		rc = qpnp_bsi_read(chip, QPNP_BSI_REG_STATUS, &reg, 1);
		if (rc) {
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
			return rc;
		}

		if (reg & QPNP_BSI_STATUS_ERROR) {
			dev_err(&chip->spmi_dev->dev, "%s: transaction error occurred, BSI error=%d\n",
				__func__, qpnp_bsi_get_bsi_error(chip));
			return -EIO;
		}

		if (!(reg & QPNP_BSI_STATUS_TX_BUSY)) {
			/* BSI TX is idle. */
			return 0;
		}

		udelay(1);
		timeout--;
	}

	rc = -ETIMEDOUT;
	dev_err(&chip->spmi_dev->dev, "%s: transaction timed out, TX_BUSY never set to 0, rc=%d\n",
		__func__, rc);

	return rc;
}

/*
 * For burst read length greater than 1, send necessary RBL and RBE BIF bus
 * commands.
 */
static int qpnp_bsi_send_burst_length(struct qpnp_bsi_chip *chip, int burst_len)
{
	int rc = 0;

	/*
	 * Send burst read length bus commands according to the following:
	 *
	 * 1                     --> No RBE or RBL
	 * 2  - 15  = x          --> RBLx
	 * 16 - 255 = 16 * y + x --> RBEy and RBLx (RBL0 not sent)
	 * 256                   --> RBL0
	 */
	if (burst_len == 256) {
		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_BC,
						BIF_CMD_RBL);
		if (rc)
			return rc;

		rc = qpnp_bsi_wait_for_tx_go(chip);
		if (rc)
			return rc;
	} else if (burst_len >= 16) {
		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_BC,
					BIF_CMD_RBE + (burst_len / 16));
		if (rc)
			return rc;

		rc = qpnp_bsi_wait_for_tx_go(chip);
		if (rc)
			return rc;
	}

	if (burst_len % 16 && burst_len > 1) {
		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_BC,
					BIF_CMD_RBL + (burst_len % 16));
		if (rc)
			return rc;

		rc = qpnp_bsi_wait_for_tx_go(chip);
		if (rc)
			return rc;
	}

	return rc;
}

/* Perform validation steps on received BIF data. */
static int qpnp_bsi_validate_rx_data(struct qpnp_bsi_chip *chip, int response,
					u8 rx2_data, bool last_word)
{
	int err = -EIO;

	if (rx2_data & QPNP_BSI_RX_SRC_LOOPBACK_FLAG) {
		dev_err(&chip->spmi_dev->dev, "%s: unexpected loopback data read, rc=%d\n",
			__func__, err);
		return err;
	}

	if (!(response & BIF_SLAVE_RD_ACK)) {
		dev_err(&chip->spmi_dev->dev, "%s: BIF register read error=0x%02X\n",
			__func__, response & BIF_SLAVE_RD_ERR);
		return err;
	}

	if (last_word && !(response & BIF_SLAVE_RD_EOT)) {
		dev_err(&chip->spmi_dev->dev, "%s: BIF register read error, last RD packet has EOT=0\n",
			__func__);
		return err;
	} else if (!last_word && (response & BIF_SLAVE_RD_EOT)) {
		dev_err(&chip->spmi_dev->dev, "%s: BIF register read error, RD packet other than last has EOT=1\n",
			__func__);
		return err;
	}

	return 0;
}

/* Performs all BIF transactions in order to utilize burst reads. */
static int qpnp_bsi_read_slave_registers(struct bif_ctrl_dev *bdev, u16 addr,
						u8 *data, int len)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int response = 0;
	unsigned long flags;
	int rc, rc2, i, burst_len;
	u8 buf[3];

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_POLL);

	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_DATA_TX_DATA);
	if (rc)
		return rc;

	while (len > 0) {
		burst_len = min(len, 256);

		rc = qpnp_bsi_send_burst_length(chip, burst_len);
		if (rc)
			return rc;

		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_ERA, addr >> 8);
		if (rc)
			return rc;

		rc = qpnp_bsi_wait_for_tx_go(chip);
		if (rc)
			return rc;

		/* Perform burst read in atomic context. */
		local_irq_save(flags);

		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_RRA,
						addr & 0xFF);
		if (rc)
			goto burst_err;

		for (i = 0; i < burst_len; i++) {
			rc = qpnp_bsi_wait_for_rx_data(chip);
			if (rc)
				goto burst_err;

			/* Read the RX_DATA bytes. */
			rc = qpnp_bsi_read(chip, QPNP_BSI_REG_RX_DATA_LOW, buf,
					   3);
			if (rc) {
				dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_read() failed, rc=%d\n",
					__func__, rc);
				goto burst_err;
			}

			response = ((buf[1] & QPNP_BSI_RX_DATA_HIGH_MASK) << 8)
					| buf[0];

			rc = qpnp_bsi_validate_rx_data(chip, response, buf[2],
					i == burst_len - 1);
			if (rc)
				goto burst_err;

			data[i] = buf[0];
		}
		local_irq_restore(flags);

		addr += burst_len;
		data += burst_len;
		len -= burst_len;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);

	return rc;

burst_err:
	local_irq_restore(flags);

	rc2 = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);
	if (rc2 < 0)
		rc = rc2;

	return rc;
}

/* Performs all BIF transactions in order to utilize burst writes. */
static int qpnp_bsi_write_slave_registers(struct bif_ctrl_dev *bdev, u16 addr,
						const u8 *data, int len)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	unsigned long flags;
	int rc, rc2, i;

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_POLL);

	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_DATA);
	if (rc)
		return rc;

	rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_ERA, addr >> 8);
	if (rc)
		return rc;

	rc = qpnp_bsi_wait_for_tx_go(chip);
	if (rc)
		return rc;

	rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_WRA, addr & 0xFF);
	if (rc)
		return rc;

	rc = qpnp_bsi_wait_for_tx_go(chip);
	if (rc)
		return rc;

	/* Perform burst write in atomic context. */
	local_irq_save(flags);

	for (i = 0; i < len; i++) {
		rc = qpnp_bsi_issue_transaction(chip, BIF_TRANS_WD, data[i]);
		if (rc)
			goto burst_err;

		rc = qpnp_bsi_wait_for_tx_go(chip);
		if (rc)
			goto burst_err;
	}

	rc = qpnp_bsi_wait_for_tx_idle(chip);
	if (rc)
		goto burst_err;

	local_irq_restore(flags);

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);

	return rc;

burst_err:
	local_irq_restore(flags);

	rc2 = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_OFF_TX_OFF);
	if (rc2 < 0)
		rc = rc2;

	return rc;
}


static int qpnp_bsi_bus_set_interrupt_mode(struct bif_ctrl_dev *bdev)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc;

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	qpnp_bsi_clear_irq_flags(chip);

	qpnp_bsi_set_com_mode(chip, QPNP_BSI_COM_MODE_IRQ);

	/*
	 * Temporarily change the bus to active state so that the EINT command
	 * can be issued.
	 */
	rc = qpnp_bsi_set_bus_state(bdev, BIF_BUS_STATE_ACTIVE);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: failed to set bus state, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_INT_TX_DATA);
	if (rc)
		return rc;

	/*
	 * Set the bus state to interrupt mode so that an RX interrupt which
	 * occurs immediately after issuing the EINT command is handled
	 * properly.
	 */
	chip->state = BIF_BUS_STATE_INTERRUPT;

	/* Send EINT bus command. */
	rc = qpnp_bsi_issue_transaction_wait_for_tx(chip, BIF_TRANS_BC,
							BIF_CMD_EINT);
	if (rc)
		return rc;

	rc = qpnp_bsi_rx_tx_config(chip, QPNP_BSI_RX_TX_STATE_RX_INT_TX_OFF);

	return rc;
}

static int qpnp_bsi_bus_set_active_mode(struct bif_ctrl_dev *bdev,
					int prev_state)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc;
	u8 buf[2];

	rc = qpnp_bsi_clear_bsi_error(chip);
	if (rc)
		return rc;

	buf[0] = QPNP_BSI_MODE_TX_PULSE_INT |
		QPNP_BSI_MODE_RX_PULSE_DATA;
	buf[1] = QPNP_BSI_TX_ENABLE | QPNP_BSI_RX_DISABLE;

	if (prev_state == BIF_BUS_STATE_INTERRUPT)
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_T_1_TAU;
	else
		buf[0] |= QPNP_BSI_MODE_TX_PULSE_T_WAKE;

	rc = qpnp_bsi_write(chip, QPNP_BSI_REG_MODE, buf, 2);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	buf[0] = QPNP_BSI_TX_CTRL_GO;
	/* Initiate BCL low pulse. */
	rc = qpnp_bsi_write(chip, QPNP_BSI_REG_TX_CTRL, buf, 1);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	switch (prev_state) {
	case BIF_BUS_STATE_INTERRUPT:
		udelay(qpnp_bsi_get_tau_us(chip) * 4);
		break;
	case BIF_BUS_STATE_STANDBY:
		udelay(qpnp_bsi_get_tau_us(chip)
			+ QPNP_BSI_MAX_SLAVE_ACTIVIATION_DELAY_US
			+ QPNP_BSI_POWER_UP_LOW_DELAY_US);
		break;
	case BIF_BUS_STATE_POWER_DOWN:
	case BIF_BUS_STATE_MASTER_DISABLED:
		msleep(QPNP_BSI_MAX_SLAVE_POWER_UP_DELAY_MS);
		break;
	}

	return rc;
}

static int qpnp_bsi_get_bus_state(struct bif_ctrl_dev *bdev)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);

	return chip->state;
}

static int qpnp_bsi_set_bus_state(struct bif_ctrl_dev *bdev, int state)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	int rc = 0;
	u8 reg;

	if (state == chip->state)
		return 0;

	if (chip->state == BIF_BUS_STATE_MASTER_DISABLED) {
		/*
		 * Enable the BSI peripheral when transitioning from a disabled
		 * bus state to any of the active bus states so that BIF
		 * transactions can take place.
		 */
		reg = QPNP_BSI_ENABLE;
		rc = qpnp_bsi_write(chip, QPNP_BSI_REG_ENABLE, &reg, 1);
		if (rc) {
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	switch (state) {
	case BIF_BUS_STATE_MASTER_DISABLED:
		/* Disable the BSI peripheral. */
		reg = QPNP_BSI_DISABLE;
		rc = qpnp_bsi_write(chip, QPNP_BSI_REG_ENABLE, &reg, 1);
		if (rc)
			dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
				__func__, rc);
		break;
	case BIF_BUS_STATE_POWER_DOWN:
		rc = qpnp_bsi_bus_transaction(bdev, BIF_TRANS_BC, BIF_CMD_PDWN);
		if (rc)
			dev_err(&chip->spmi_dev->dev, "%s: failed to enable power down mode, rc=%d\n",
				__func__, rc);
		break;
	case BIF_BUS_STATE_STANDBY:
		rc = qpnp_bsi_bus_transaction(bdev, BIF_TRANS_BC, BIF_CMD_STBY);
		if (rc)
			dev_err(&chip->spmi_dev->dev, "%s: failed to enable standby mode, rc=%d\n",
				__func__, rc);
		break;
	case BIF_BUS_STATE_ACTIVE:
		rc = qpnp_bsi_bus_set_active_mode(bdev, chip->state);
		if (rc)
			dev_err(&chip->spmi_dev->dev, "%s: failed to enable active mode, rc=%d\n",
				__func__, rc);
		break;
	case BIF_BUS_STATE_INTERRUPT:
		/*
		 * qpnp_bsi_bus_set_interrupt_mode() internally sets
		 * chip->state = BIF_BUS_STATE_INTERRUPT immediately before
		 * issuing the EINT command.
		 */
		rc = qpnp_bsi_bus_set_interrupt_mode(bdev);
		if (rc) {
			dev_err(&chip->spmi_dev->dev, "%s: failed to enable interrupt mode, rc=%d\n",
				__func__, rc);
		} else if (chip->state == BIF_BUS_STATE_ACTIVE) {
			/*
			 * A slave interrupt was received immediately after
			 * issuing the EINT command.  Therefore, stay in active
			 * communication mode.
			 */
			state = BIF_BUS_STATE_ACTIVE;
		}
		break;
	default:
		rc = -EINVAL;
		dev_err(&chip->spmi_dev->dev, "%s: invalid state=%d\n",
			__func__, state);
	}

	if (!rc)
		chip->state = state;

	return rc;
}

/* Returns the smallest tau_bif that is greater than or equal to period_ns. */
static int qpnp_bsi_tau_bif_higher(int period_ns, int sample_mask)
{
	const int *supported_period_ns =
			(sample_mask == QPNP_BSI_TAU_CONFIG_SAMPLE_4X ?
				qpnp_bsi_tau_period.period_4x_ns :
				qpnp_bsi_tau_period.period_8x_ns);
	int smallest_tau_bif = INT_MAX;
	int i;

	for (i = QPNP_BSI_NUM_CLOCK_PERIODS - 1; i >= 0; i--) {
		if (period_ns <= supported_period_ns[i]) {
			smallest_tau_bif = supported_period_ns[i];
			break;
		}
	}

	return smallest_tau_bif;
}

/* Returns the largest tau_bif that is less than or equal to period_ns. */
static int qpnp_bsi_tau_bif_lower(int period_ns, int sample_mask)
{
	const int *supported_period_ns =
			(sample_mask == QPNP_BSI_TAU_CONFIG_SAMPLE_4X ?
				qpnp_bsi_tau_period.period_4x_ns :
				qpnp_bsi_tau_period.period_8x_ns);
	int largest_tau_bif = 0;
	int i;

	for (i = 0; i < QPNP_BSI_NUM_CLOCK_PERIODS; i++) {
		if (period_ns >= supported_period_ns[i]) {
			largest_tau_bif = supported_period_ns[i];
			break;
		}
	}

	return largest_tau_bif;
}

/*
 * Moves period_ns into allowed range and then sets tau bif to the period that
 * is greater than or equal to period_ns.
 */
static int qpnp_bsi_set_tau_bif(struct qpnp_bsi_chip *chip, int period_ns)
{
	const int *supported_period_ns =
		(chip->tau_sampling_mask == QPNP_BSI_TAU_CONFIG_SAMPLE_4X ?
			qpnp_bsi_tau_period.period_4x_ns :
			qpnp_bsi_tau_period.period_8x_ns);
	int idx = 0;
	int i, rc;
	u8 reg;

	if (period_ns < chip->bdesc.bus_clock_min_ns)
		period_ns = chip->bdesc.bus_clock_min_ns;
	else if (period_ns > chip->bdesc.bus_clock_max_ns)
		period_ns = chip->bdesc.bus_clock_max_ns;

	for (i = QPNP_BSI_NUM_CLOCK_PERIODS - 1; i >= 0; i--) {
		if (period_ns <= supported_period_ns[i]) {
			idx = i;
			break;
		}
	}

	/* Set the tau BIF clock period and sampling rate. */
	reg = chip->tau_sampling_mask | idx;
	rc = qpnp_bsi_write(chip, QPNP_BSI_REG_TAU_CONFIG, &reg, 1);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_bsi_write() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	chip->tau_index = idx;

	return 0;
}

static int qpnp_bsi_get_bus_period(struct bif_ctrl_dev *bdev)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);

	return qpnp_bsi_get_tau_ns(chip);
}

static int qpnp_bsi_set_bus_period(struct bif_ctrl_dev *bdev, int period_ns)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);

	return qpnp_bsi_set_tau_bif(chip, period_ns);
}

static int qpnp_bsi_get_battery_rid(struct bif_ctrl_dev *bdev)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	struct qpnp_vadc_result adc_result;
	int rid_ohm, vid_uV, rc;
	s64 temp;

	if (chip->batt_id_adc_channel >= ADC_MAX_NUM) {
		dev_err(&chip->spmi_dev->dev, "%s: no ADC channel specified for Rid measurement\n",
			__func__);
		return -ENXIO;
	}

	rc = qpnp_vadc_read(chip->vadc_dev, chip->batt_id_adc_channel,
								&adc_result);
	if (!rc) {
		vid_uV = adc_result.physical;

		if (chip->vid_ref_uV - vid_uV <= 0) {
			rid_ohm = INT_MAX;
		} else {
			temp = (s64)chip->r_pullup_ohm * (s64)vid_uV;
			do_div(temp, chip->vid_ref_uV - vid_uV);
			if (temp > INT_MAX)
				rid_ohm = INT_MAX;
			else
				rid_ohm = temp;
		}
	} else {
		dev_err(&chip->spmi_dev->dev, "%s: qpnp_vadc_read(%d) failed, rc=%d\n",
			__func__, chip->batt_id_adc_channel, rc);
		rid_ohm = rc;
	}

	return rid_ohm;
}

/*
 * Returns 1 if a battery pack is present on the BIF bus, 0 if a battery pack
 * is not present, or errno if detection fails.
 *
 * Battery detection is based upon the idle BCL voltage.
 */
static int qpnp_bsi_get_battery_presence(struct bif_ctrl_dev *bdev)
{
	struct qpnp_bsi_chip *chip = bdev_get_drvdata(bdev);
	u8 reg = 0x00;
	int rc;

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->batt_id_stat_addr, &reg, 1);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: spmi_ext_register_readl() failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	return !!(reg & QPNP_SMBB_BAT_IF_BATT_PRES_MASK);
}

static struct bif_ctrl_ops qpnp_bsi_ops = {
	.bus_transaction	= qpnp_bsi_bus_transaction,
	.bus_transaction_query	= qpnp_bsi_bus_transaction_query,
	.bus_transaction_read	= qpnp_bsi_bus_transaction_read,
	.get_bus_state		= qpnp_bsi_get_bus_state,
	.set_bus_state		= qpnp_bsi_set_bus_state,
	.get_bus_period		= qpnp_bsi_get_bus_period,
	.set_bus_period		= qpnp_bsi_set_bus_period,
	.read_slave_registers	= qpnp_bsi_read_slave_registers,
	.write_slave_registers	= qpnp_bsi_write_slave_registers,
	.get_battery_rid	= qpnp_bsi_get_battery_rid,
	.get_battery_presence	= qpnp_bsi_get_battery_presence,
};

/* Load all BSI properties from device tree. */
static int qpnp_bsi_parse_dt(struct qpnp_bsi_chip *chip,
			struct spmi_device *spmi)
{
	struct device *dev = &spmi->dev;
	struct device_node *node = spmi->dev.of_node;
	struct resource *res;
	int rc, temp;

	chip->batt_id_adc_channel = ADC_MAX_NUM;
	rc = of_property_read_u32(node, "qcom,channel-num",
				  &chip->batt_id_adc_channel);
	if (!rc && (chip->batt_id_adc_channel < 0
			|| chip->batt_id_adc_channel >= ADC_MAX_NUM)) {
		dev_err(dev, "%s: invalid qcom,channel-num=%d specified\n",
			__func__, chip->batt_id_adc_channel);
		return -EINVAL;
	}

	chip->r_pullup_ohm = QPNP_BSI_DEFAULT_PULLUP_OHM;
	rc = of_property_read_u32(node, "qcom,pullup-ohms",
					&chip->r_pullup_ohm);
	if (!rc && (chip->r_pullup_ohm < QPNP_BSI_MIN_PULLUP_OHM ||
			chip->r_pullup_ohm > QPNP_BSI_MAX_PULLUP_OHM)) {
		dev_err(dev, "%s: invalid qcom,pullup-ohms=%d property value\n",
			__func__, chip->r_pullup_ohm);
		return -EINVAL;
	}

	chip->vid_ref_uV = QPNP_BSI_DEFAULT_VID_REF_UV;
	rc = of_property_read_u32(node, "qcom,vref-microvolts",
					&chip->vid_ref_uV);
	if (!rc && (chip->vid_ref_uV < QPNP_BSI_MIN_VID_REF_UV ||
			chip->vid_ref_uV > QPNP_BSI_MAX_VID_REF_UV)) {
		dev_err(dev, "%s: invalid qcom,vref-microvolts=%d property value\n",
			__func__, chip->vid_ref_uV);
		return -EINVAL;
	}

	res = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM, "bsi-base");
	if (!res) {
		dev_err(dev, "%s: node is missing BSI base address\n",
			__func__);
		return -EINVAL;
	}
	chip->base_addr = res->start;

	res = spmi_get_resource_byname(spmi, NULL, IORESOURCE_MEM,
		"batt-id-status");
	if (!res) {
		dev_err(dev, "%s: node is missing BATT_ID status address\n",
			__func__);
		return -EINVAL;
	}
	chip->batt_id_stat_addr = res->start;

	chip->bdesc.name = spmi_get_primary_dev_name(spmi);
	if (!chip->bdesc.name) {
		dev_err(dev, "%s: label binding undefined for node %s\n",
			__func__, spmi->dev.of_node->full_name);
		return -EINVAL;
	}

	/* Use maximum range by default. */
	chip->bdesc.bus_clock_min_ns	= QPNP_BSI_MIN_CLOCK_SPEED_NS;
	chip->bdesc.bus_clock_max_ns	= QPNP_BSI_MAX_CLOCK_SPEED_NS;
	chip->tau_sampling_mask		= QPNP_BSI_TAU_CONFIG_SAMPLE_4X;

	rc = of_property_read_u32(node, "qcom,sample-rate", &temp);
	if (rc == 0) {
		if (temp == 4) {
			chip->tau_sampling_mask = QPNP_BSI_TAU_CONFIG_SAMPLE_4X;
		} else if (temp == 8) {
			chip->tau_sampling_mask = QPNP_BSI_TAU_CONFIG_SAMPLE_8X;
		} else {
			dev_err(dev, "%s: invalid qcom,sample-rate=%d.  Only values of 4 and 8 are supported.\n",
				__func__, temp);
			return -EINVAL;
		}
	}

	rc = of_property_read_u32(node, "qcom,min-clock-period", &temp);
	if (rc == 0)
		chip->bdesc.bus_clock_min_ns = qpnp_bsi_tau_bif_higher(temp,
						chip->tau_sampling_mask);

	rc = of_property_read_u32(node, "qcom,max-clock-period", &temp);
	if (rc == 0)
		chip->bdesc.bus_clock_max_ns = qpnp_bsi_tau_bif_lower(temp,
						chip->tau_sampling_mask);

	if (chip->bdesc.bus_clock_min_ns > chip->bdesc.bus_clock_max_ns) {
		dev_err(dev, "%s: invalid qcom,min/max-clock-period.\n",
			__func__);
		return -EINVAL;
	}

	chip->irq[QPNP_BSI_IRQ_ERR] = spmi_get_irq_byname(spmi, NULL, "err");
	if (chip->irq[QPNP_BSI_IRQ_ERR] < 0) {
		dev_err(dev, "%s: node is missing err irq\n", __func__);
		return chip->irq[QPNP_BSI_IRQ_ERR];
	}

	chip->irq[QPNP_BSI_IRQ_RX] = spmi_get_irq_byname(spmi, NULL, "rx");
	if (chip->irq[QPNP_BSI_IRQ_RX] < 0) {
		dev_err(dev, "%s: node is missing rx irq\n", __func__);
		return chip->irq[QPNP_BSI_IRQ_RX];
	}

	chip->irq[QPNP_BSI_IRQ_TX] = spmi_get_irq_byname(spmi, NULL, "tx");
	if (chip->irq[QPNP_BSI_IRQ_TX] < 0) {
		dev_err(dev, "%s: node is missing tx irq\n", __func__);
		return chip->irq[QPNP_BSI_IRQ_TX];
	}

	chip->batt_present_irq = spmi_get_irq_byname(spmi, NULL,
		"batt-present");
	if (chip->batt_present_irq < 0) {
		dev_err(dev, "%s: node is missing batt-present irq\n",
			__func__);
		return chip->batt_present_irq;
	}

	return rc;
}

/* Request all BSI and battery presence IRQs and set them as wakeable. */
static int qpnp_bsi_init_irqs(struct qpnp_bsi_chip *chip,
			struct device *dev)
{
	int rc;

	rc = devm_request_irq(dev, chip->irq[QPNP_BSI_IRQ_ERR],
			qpnp_bsi_isr, IRQF_TRIGGER_RISING, "bsi-err", chip);
	if (rc < 0) {
		dev_err(dev, "%s: request for bsi-err irq %d failed, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_ERR], rc);
		return rc;
	}

	rc = irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_ERR], 1);
	if (rc < 0) {
		dev_err(dev, "%s: unable to set bsi-err irq %d as wakeable, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_ERR], rc);
		return rc;
	}

	rc = devm_request_irq(dev, chip->irq[QPNP_BSI_IRQ_RX],
			qpnp_bsi_isr, IRQF_TRIGGER_RISING, "bsi-rx", chip);
	if (rc < 0) {
		dev_err(dev, "%s: request for bsi-rx irq %d failed, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_RX], rc);
		goto set_unwakeable_irq_err;
	}

	rc = irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_RX], 1);
	if (rc < 0) {
		dev_err(dev, "%s: unable to set bsi-rx irq %d as wakeable, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_RX], rc);
		goto set_unwakeable_irq_err;
	}

	rc = devm_request_irq(dev, chip->irq[QPNP_BSI_IRQ_TX],
			qpnp_bsi_isr, IRQF_TRIGGER_RISING, "bsi-tx", chip);
	if (rc < 0) {
		dev_err(dev, "%s: request for bsi-tx irq %d failed, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_TX], rc);
		goto set_unwakeable_irq_rx;
	}

	rc = irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_TX], 1);
	if (rc < 0) {
		dev_err(dev, "%s: unable to set bsi-tx irq %d as wakeable, rc=%d\n",
			__func__, chip->irq[QPNP_BSI_IRQ_TX], rc);
		goto set_unwakeable_irq_rx;
	}

	rc = devm_request_threaded_irq(dev, chip->batt_present_irq, NULL,
		qpnp_bsi_batt_present_isr,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_SHARED
			| IRQF_ONESHOT,
		"bsi-batt-present", chip);
	if (rc < 0) {
		dev_err(dev, "%s: request for bsi-batt-present irq %d failed, rc=%d\n",
			__func__, chip->batt_present_irq, rc);
		goto set_unwakeable_irq_tx;
	}

	rc = irq_set_irq_wake(chip->batt_present_irq, 1);
	if (rc < 0) {
		dev_err(dev, "%s: unable to set bsi-batt-present irq %d as wakeable, rc=%d\n",
			__func__, chip->batt_present_irq, rc);
		goto set_unwakeable_irq_tx;
	}

	return rc;

set_unwakeable_irq_tx:
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_TX], 0);
set_unwakeable_irq_rx:
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_RX], 0);
set_unwakeable_irq_err:
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_ERR], 0);
	return rc;
}

static void qpnp_bsi_cleanup_irqs(struct qpnp_bsi_chip *chip)
{
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_ERR], 0);
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_RX], 0);
	irq_set_irq_wake(chip->irq[QPNP_BSI_IRQ_TX], 0);
	irq_set_irq_wake(chip->batt_present_irq, 0);
}

static int qpnp_bsi_probe(struct spmi_device *spmi)
{
	struct device *dev = &spmi->dev;
	struct qpnp_bsi_chip *chip;
	int rc;
	u8 type[2];

	if (!spmi->dev.of_node) {
		dev_err(dev, "%s: device node missing\n", __func__);
		return -ENODEV;
	}

	chip = devm_kzalloc(dev, sizeof(struct qpnp_bsi_chip), GFP_KERNEL);
	if (!chip) {
		dev_err(dev, "%s: Can't allocate qpnp_bsi\n", __func__);
		return -ENOMEM;
	}

	rc = qpnp_bsi_parse_dt(chip, spmi);
	if (rc) {
		dev_err(dev, "%s: device tree parsing failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	INIT_WORK(&chip->slave_irq_work, qpnp_bsi_slave_irq_work);

	rc = qpnp_bsi_init_irqs(chip, dev);
	if (rc) {
		dev_err(dev, "%s: IRQ initialization failed, rc=%d\n",
			__func__, rc);
		return rc;
	}

	chip->spmi_dev		= spmi;
	chip->bdesc.ops		= &qpnp_bsi_ops;
	chip->state		= BIF_BUS_STATE_MASTER_DISABLED;
	chip->com_mode		= QPNP_BSI_COM_MODE_IRQ;

	rc = qpnp_bsi_read(chip, QPNP_BSI_REG_TYPE, type, 2);
	if (rc) {
		dev_err(dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		goto cleanup_irqs;
	}

	if (type[0] != QPNP_BSI_TYPE || type[1] != QPNP_BSI_SUBTYPE) {
		dev_err(dev, "%s: BSI peripheral is not present; type=0x%02X, subtype=0x%02X\n",
			__func__, type[0], type[1]);
		rc = -ENODEV;
		goto cleanup_irqs;
	}

	/* Ensure that ADC channel is available if it was specified. */
	if (chip->batt_id_adc_channel < ADC_MAX_NUM) {
		chip->vadc_dev = qpnp_get_vadc(dev, "bsi");
		if (IS_ERR(chip->vadc_dev)) {
			rc = PTR_ERR(chip->vadc_dev);
			if (rc != -EPROBE_DEFER)
				pr_err("missing vadc property, rc=%d\n", rc);
			/* Probe retry, do not print an error message */
			goto cleanup_irqs;
		}
	}

	rc = qpnp_bsi_set_tau_bif(chip, chip->bdesc.bus_clock_min_ns);
	if (rc) {
		dev_err(dev, "%s: qpnp_bsi_set_tau_bif() failed, rc=%d\n",
			__func__, rc);
		goto cleanup_irqs;
	}

	chip->bdev = bif_ctrl_register(&chip->bdesc, dev, chip,
					spmi->dev.of_node);
	if (IS_ERR(chip->bdev)) {
		rc = PTR_ERR(chip->bdev);
		dev_err(dev, "%s: bif_ctrl_register failed, rc=%d\n",
			__func__, rc);
		goto cleanup_irqs;
	}

	dev_set_drvdata(dev, chip);

	return rc;

cleanup_irqs:
	qpnp_bsi_cleanup_irqs(chip);
	return rc;
}

static int qpnp_bsi_remove(struct spmi_device *spmi)
{
	struct qpnp_bsi_chip *chip = dev_get_drvdata(&spmi->dev);
	dev_set_drvdata(&spmi->dev, NULL);

	if (chip) {
		bif_ctrl_unregister(chip->bdev);
		qpnp_bsi_cleanup_irqs(chip);
	}

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = QPNP_BSI_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_bsi_id[] = {
	{ QPNP_BSI_DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spmi, qpnp_bsi_id);

static struct spmi_driver qpnp_bsi_driver = {
	.driver = {
		.name		= QPNP_BSI_DRIVER_NAME,
		.of_match_table	= spmi_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= qpnp_bsi_probe,
	.remove		= qpnp_bsi_remove,
	.id_table	= qpnp_bsi_id,
};

static int __init qpnp_bsi_init(void)
{
	return spmi_driver_register(&qpnp_bsi_driver);
}

static void __exit qpnp_bsi_exit(void)
{
	spmi_driver_unregister(&qpnp_bsi_driver);
}

MODULE_DESCRIPTION("QPNP PMIC BSI driver");
MODULE_LICENSE("GPL v2");

arch_initcall(qpnp_bsi_init);
module_exit(qpnp_bsi_exit);

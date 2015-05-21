/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/input/matrix_keypad.h>
#include <linux/spmi.h>

#define QPNP_MAX_ROWS			10
#define QPNP_MAX_COLS			8
#define QPNP_MIN_ROWS			2
#define QPNP_MIN_COLS			1
#define QPNP_ROW_SHIFT			3
#define QPNP_MATRIX_MAX_SIZE		(QPNP_MAX_ROWS * QPNP_MAX_COLS)

/* in ms */
#define MAX_SCAN_DELAY			128
#define MIN_SCAN_DELAY			1
#define KEYP_DEFAULT_SCAN_DELAY		32

/* in ns */
#define MAX_ROW_HOLD_DELAY		250000
#define MIN_ROW_HOLD_DELAY		31250

/* in ms */
#define MAX_DEBOUNCE_TIME		20
#define MIN_DEBOUNCE_TIME		5
#define KEYP_DEFAULT_DEBOUNCE		15

/* register offsets */
#define KEYP_STATUS(base)		(base + 0x08)
#define KEYP_SIZE_CTRL(base)		(base + 0x40)
#define KEYP_SCAN_CTRL(base)		(base + 0x42)
#define KEYP_FSM_CNTL(base)		(base + 0x44)
#define KEYP_EN_CTRL(base)		(base + 0x46)

#define KEYP_CTRL_KEYP_EN		BIT(7)
#define KEYP_CTRL_EVNTS			BIT(0)
#define KEYP_CTRL_EVNTS_MASK		0x3

#define KEYP_SIZE_COLS_SHIFT		4
#define KEYP_SIZE_COLS_MASK		0x70
#define KEYP_SIZE_ROWS_MASK		0x0F

#define KEYP_SCAN_DBC_MASK		0x03
#define KEYP_SCAN_SCNP_MASK		0x38
#define KEYP_SCAN_ROWP_MASK		0xC0
#define KEYP_SCAN_SCNP_SHIFT		3
#define KEYP_SCAN_ROWP_SHIFT		6

#define KEYP_CTRL_SCAN_ROWS_BITS	0x7

#define KEYP_SCAN_DBOUNCE_SHIFT		1
#define KEYP_SCAN_PAUSE_SHIFT		3
#define KEYP_SCAN_ROW_HOLD_SHIFT	6

#define KEYP_FSM_READ_EN		BIT(0)

/* bits of these registers represent
 * '0' for key press
 * '1' for key release
 */
#define KEYP_RECENT_DATA(base)		(base + 0x7C)
#define KEYP_OLD_DATA(base)		(base + 0x5C)

#define KEYP_CLOCK_FREQ			32768

struct qpnp_kp {
	const struct matrix_keymap_data *keymap_data;
	struct input_dev *input;
	struct spmi_device *spmi;

	int key_sense_irq;
	int key_stuck_irq;
	u16 base;

	u32 num_rows;
	u32 num_cols;
	u32 debounce_ms;
	u32 row_hold_ns;
	u32 scan_delay_ms;
	bool wakeup;
	bool rep;

	unsigned short keycodes[QPNP_MATRIX_MAX_SIZE];

	u16 keystate[QPNP_MAX_ROWS];
	u16 stuckstate[QPNP_MAX_ROWS];
};

static int qpnp_kp_write_u8(struct qpnp_kp *kp, u8 data, u16 reg)
{
	int rc;

	rc = spmi_ext_register_writel(kp->spmi->ctrl, kp->spmi->sid,
							reg, &data, 1);
	if (rc < 0)
		dev_err(&kp->spmi->dev,
			"Error writing to address: %X - ret %d\n", reg, rc);

	return rc;
}

static int qpnp_kp_read(struct qpnp_kp *kp,
				u8 *data, u16 reg, unsigned num_bytes)
{
	int rc;

	rc = spmi_ext_register_readl(kp->spmi->ctrl, kp->spmi->sid,
						reg, data, num_bytes);
	if (rc < 0)
		dev_err(&kp->spmi->dev,
			"Error reading from address : %X - ret %d\n", reg, rc);

	return rc;
}

static int qpnp_kp_read_u8(struct qpnp_kp *kp, u8 *data, u16 reg)
{
	int rc;

	rc = qpnp_kp_read(kp, data, reg, 1);
	if (rc < 0)
		dev_err(&kp->spmi->dev, "Error reading qpnp: %X - ret %d\n",
				reg, rc);
	return rc;
}

static u8 qpnp_col_state(struct qpnp_kp *kp, u8 col)
{
	/* all keys pressed on that particular row? */
	if (col == 0x00)
		return 1 << kp->num_cols;
	else
		return col & ((1 << kp->num_cols) - 1);
}

/*
 * Synchronous read protocol
 *
 * 1. Write '1' to ReadState bit in KEYP_FSM_CNTL register
 * 2. Wait 2*32KHz clocks, so that HW can successfully enter read mode
 *    synchronously
 * 3. Read rows in old array first if events are more than one
 * 4. Read rows in recent array
 * 5. Wait 4*32KHz clocks
 * 6. Write '0' to ReadState bit of KEYP_FSM_CNTL register so that hw can
 *    synchronously exit read mode.
 */
static int qpnp_sync_read(struct qpnp_kp *kp, bool enable)
{
	int rc;
	u8 fsm_ctl;

	rc = qpnp_kp_read_u8(kp, &fsm_ctl, KEYP_FSM_CNTL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
				"Error reading KEYP_FSM_CNTL reg, rc=%d\n", rc);
		return rc;
	}

	if (enable)
		fsm_ctl |= KEYP_FSM_READ_EN;
	else
		fsm_ctl &= ~KEYP_FSM_READ_EN;

	rc = qpnp_kp_write_u8(kp, fsm_ctl, KEYP_FSM_CNTL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
				"Error writing KEYP_FSM_CNTL reg, rc=%d\n", rc);
		return rc;
	}

	/* 2 * 32KHz clocks */
	udelay((2 * DIV_ROUND_UP(USEC_PER_SEC, KEYP_CLOCK_FREQ)) + 1);

	return rc;
}

static int qpnp_kp_read_data(struct qpnp_kp *kp, u16 *state,
					u16 data_reg, int read_rows)
{
	int rc, row;
	u8 new_data[QPNP_MAX_ROWS];

	/*
	 * Check if last row will be scanned. If not, scan to clear key event
	 * counter
	 */
	if (kp->num_rows < QPNP_MAX_ROWS) {
		rc = qpnp_kp_read_u8(kp, &new_data[QPNP_MAX_ROWS - 1],
					data_reg + (QPNP_MAX_ROWS - 1) * 2);
		if (rc)
			return rc;
	}

	for (row = 0; row < kp->num_rows; row++) {
		rc = qpnp_kp_read_u8(kp, &new_data[row], data_reg + row * 2);
		if (rc)
			return rc;

		dev_dbg(&kp->spmi->dev, "new_data[%d] = %d\n", row,
					new_data[row]);
		state[row] = qpnp_col_state(kp, new_data[row]);
	}

	return 0;
}

static int qpnp_kp_read_matrix(struct qpnp_kp *kp, u16 *new_state,
					 u16 *old_state)
{
	int rc, read_rows;

	read_rows = kp->num_rows;

	rc = qpnp_sync_read(kp, true);
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error setting the FSM read enable bit rc=%d\n", rc);
		return rc;
	}

	if (old_state) {
		rc = qpnp_kp_read_data(kp, old_state, KEYP_OLD_DATA(kp->base),
							read_rows);
		if (rc < 0) {
			dev_err(&kp->spmi->dev,
				"Error reading KEYP_OLD_DATA, rc=%d\n", rc);
			return rc;
		}
	}

	rc = qpnp_kp_read_data(kp, new_state, KEYP_RECENT_DATA(kp->base),
						 read_rows);
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_RECENT_DATA, rc=%d\n", rc);
		return rc;
	}

	/* 4 * 32KHz clocks */
	udelay((4 * DIV_ROUND_UP(USEC_PER_SEC, KEYP_CLOCK_FREQ)) + 1);

	rc = qpnp_sync_read(kp, false);
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error resetting the FSM read enable bit rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static void __qpnp_kp_scan_matrix(struct qpnp_kp *kp, u16 *new_state,
					 u16 *old_state)
{
	int row, col, code;

	for (row = 0; row < kp->num_rows; row++) {
		int bits_changed = new_state[row] ^ old_state[row];

		if (!bits_changed)
			continue;

		for (col = 0; col < kp->num_cols; col++) {
			if (!(bits_changed & (1 << col)))
				continue;

			dev_dbg(&kp->spmi->dev, "key [%d:%d] %s\n", row, col,
					!(new_state[row] & (1 << col)) ?
					"pressed" : "released");
			code = MATRIX_SCAN_CODE(row, col, QPNP_ROW_SHIFT);
			input_event(kp->input, EV_MSC, MSC_SCAN, code);
			input_report_key(kp->input,
					kp->keycodes[code],
					!(new_state[row] & (1 << col)));
			input_sync(kp->input);
		}
	}
}

static bool qpnp_detect_ghost_keys(struct qpnp_kp *kp, u16 *new_state)
{
	int row, found_first = -1;
	u16 check, row_state;

	check = 0;
	for (row = 0; row < kp->num_rows; row++) {
		row_state = (~new_state[row]) &
				 ((1 << kp->num_cols) - 1);

		if (hweight16(row_state) > 1) {
			if (found_first == -1)
				found_first = row;
			if (check & row_state) {
				dev_dbg(&kp->spmi->dev,
					"detected ghost key row[%d],row[%d]\n",
					found_first, row);
				return true;
			}
		}
		check |= row_state;
	}
	return false;
}

static int qpnp_kp_scan_matrix(struct qpnp_kp *kp, unsigned int events)
{
	u16 new_state[QPNP_MAX_ROWS];
	u16 old_state[QPNP_MAX_ROWS];
	int rc;
	switch (events) {
	case 0x1:
		rc = qpnp_kp_read_matrix(kp, new_state, NULL);
		if (rc < 0)
			return rc;

		/* detecting ghost key is not an error */
		if (qpnp_detect_ghost_keys(kp, new_state))
			return 0;
		__qpnp_kp_scan_matrix(kp, new_state, kp->keystate);
		memcpy(kp->keystate, new_state, sizeof(new_state));
	break;
	case 0x3: /* two events - eventcounter is gray-coded */
		rc = qpnp_kp_read_matrix(kp, new_state, old_state);
		if (rc < 0)
			return rc;

		__qpnp_kp_scan_matrix(kp, old_state, kp->keystate);
		__qpnp_kp_scan_matrix(kp, new_state, old_state);
		memcpy(kp->keystate, new_state, sizeof(new_state));
	break;
	case 0x2:
		dev_dbg(&kp->spmi->dev, "Some key events were lost\n");
		rc = qpnp_kp_read_matrix(kp, new_state, old_state);
		if (rc < 0)
			return rc;
		__qpnp_kp_scan_matrix(kp, old_state, kp->keystate);
		__qpnp_kp_scan_matrix(kp, new_state, old_state);
		memcpy(kp->keystate, new_state, sizeof(new_state));
	break;
	default:
		rc = -EINVAL;
	}
	return rc;
}

/*
 * NOTE: We are reading recent and old data registers blindly
 * whenever key-stuck interrupt happens, because events counter doesn't
 * get updated when this interrupt happens due to key stuck doesn't get
 * considered as key state change.
 *
 * We are not using old data register contents after they are being read
 * because it might report the key which was pressed before the key being stuck
 * as stuck key because it's pressed status is stored in the old data
 * register.
 */
static irqreturn_t qpnp_kp_stuck_irq(int irq, void *data)
{
	u16 new_state[QPNP_MAX_ROWS];
	u16 old_state[QPNP_MAX_ROWS];
	int rc;
	struct qpnp_kp *kp = data;

	rc = qpnp_kp_read_matrix(kp, new_state, old_state);
	if (rc < 0) {
		dev_err(&kp->spmi->dev, "failed to read keypad matrix\n");
		return IRQ_HANDLED;
	}

	__qpnp_kp_scan_matrix(kp, new_state, kp->stuckstate);

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_kp_irq(int irq, void *data)
{
	struct qpnp_kp *kp = data;
	u8 ctrl_val, events;
	int rc;

	rc = qpnp_kp_read_u8(kp, &ctrl_val, KEYP_STATUS(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_STATUS register\n");
		return IRQ_HANDLED;
	}

	events = ctrl_val & KEYP_CTRL_EVNTS_MASK;

	rc = qpnp_kp_scan_matrix(kp, events);
	if (rc < 0)
		dev_err(&kp->spmi->dev, "failed to scan matrix\n");

	return IRQ_HANDLED;
}

static int qpnp_kpd_init(struct qpnp_kp *kp)
{
	int bits, rc, cycles;
	u8 kpd_scan_cntl, kpd_size_cntl;

	/* Configure the SIZE register, #rows and #columns */
	rc = qpnp_kp_read_u8(kp, &kpd_size_cntl, KEYP_SIZE_CTRL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_SIZE_CTRL reg, rc=%d\n", rc);
		return rc;
	}

	kpd_size_cntl &= (~KEYP_SIZE_COLS_MASK | ~KEYP_SIZE_ROWS_MASK);
	kpd_size_cntl |= (((kp->num_cols - 1) << KEYP_SIZE_COLS_SHIFT) &
							KEYP_SIZE_COLS_MASK);
	kpd_size_cntl |= ((kp->num_rows - 1) & KEYP_SIZE_ROWS_MASK);

	rc = qpnp_kp_write_u8(kp, kpd_size_cntl, KEYP_SIZE_CTRL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error writing to KEYP_SIZE_CTRL reg, rc=%d\n", rc);
		return rc;
	}

	/* Configure the SCAN CTL register, debounce, row pause, scan delay */
	rc = qpnp_kp_read_u8(kp, &kpd_scan_cntl, KEYP_SCAN_CTRL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_SCAN_CTRL reg, rc=%d\n", rc);
		return rc;
	}

	kpd_scan_cntl &= (~KEYP_SCAN_DBC_MASK | ~KEYP_SCAN_SCNP_MASK |
						~KEYP_SCAN_ROWP_MASK);
	kpd_scan_cntl |= (((kp->debounce_ms / 5) - 1) & KEYP_SCAN_DBC_MASK);

	bits = fls(kp->scan_delay_ms) - 1;
	kpd_scan_cntl |= ((bits << KEYP_SCAN_SCNP_SHIFT) & KEYP_SCAN_SCNP_MASK);

	/* Row hold time is a multiple of 32KHz cycles. */
	cycles = (kp->row_hold_ns * KEYP_CLOCK_FREQ) / NSEC_PER_SEC;
	if (cycles)
		cycles = ilog2(cycles);
	kpd_scan_cntl |= ((cycles << KEYP_SCAN_ROW_HOLD_SHIFT) &
							KEYP_SCAN_ROWP_MASK);

	rc = qpnp_kp_write_u8(kp, kpd_scan_cntl, KEYP_SCAN_CTRL(kp->base));
	if (rc)
		dev_err(&kp->spmi->dev,
			"Error writing KEYP_SCAN reg, rc=%d\n", rc);

	return rc;
}

static int qpnp_kp_enable(struct qpnp_kp *kp)
{
	int rc;
	u8 kpd_cntl;

	rc = qpnp_kp_read_u8(kp, &kpd_cntl, KEYP_EN_CTRL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_EN_CTRL reg, rc=%d\n", rc);
		return rc;
	}

	kpd_cntl |= KEYP_CTRL_KEYP_EN;

	rc = qpnp_kp_write_u8(kp, kpd_cntl, KEYP_EN_CTRL(kp->base));
	if (rc < 0)
		dev_err(&kp->spmi->dev,
			"Error writing KEYP_CTRL reg, rc=%d\n", rc);

	return rc;
}

static int qpnp_kp_disable(struct qpnp_kp *kp)
{
	int rc;
	u8 kpd_cntl;

	rc = qpnp_kp_read_u8(kp, &kpd_cntl, KEYP_EN_CTRL(kp->base));
	if (rc < 0) {
		dev_err(&kp->spmi->dev,
			"Error reading KEYP_EN_CTRL reg, rc=%d\n", rc);
		return rc;
	}

	kpd_cntl &= ~KEYP_CTRL_KEYP_EN;

	rc = qpnp_kp_write_u8(kp, kpd_cntl, KEYP_EN_CTRL(kp->base));
	if (rc < 0)
		dev_err(&kp->spmi->dev,
			"Error writing KEYP_CTRL reg, rc=%d\n", rc);

	return rc;
}

static int qpnp_kp_open(struct input_dev *dev)
{
	struct qpnp_kp *kp = input_get_drvdata(dev);

	return qpnp_kp_enable(kp);
}

static void qpnp_kp_close(struct input_dev *dev)
{
	struct qpnp_kp *kp = input_get_drvdata(dev);

	qpnp_kp_disable(kp);
}

static int qpnp_keypad_parse_dt(struct qpnp_kp *kp)
{
	struct matrix_keymap_data *keymap_data;
	int rc, keymap_len, i;
	u32 *keymap;
	const __be32 *map;

	rc = of_property_read_u32(kp->spmi->dev.of_node,
				"keypad,num-rows", &kp->num_rows);
	if (rc) {
		dev_err(&kp->spmi->dev, "Unable to parse 'num-rows'\n");
		return rc;
	}

	rc = of_property_read_u32(kp->spmi->dev.of_node,
				"keypad,num-cols", &kp->num_cols);
	if (rc) {
		dev_err(&kp->spmi->dev, "Unable to parse 'num-cols'\n");
		return rc;
	}

	rc = of_property_read_u32(kp->spmi->dev.of_node,
				"qcom,scan-delay-ms", &kp->scan_delay_ms);
	if (rc && rc != -EINVAL) {
		dev_err(&kp->spmi->dev, "Unable to parse 'scan-delay-ms'\n");
		return rc;
	}

	rc = of_property_read_u32(kp->spmi->dev.of_node,
				"qcom,row-hold-ns", &kp->row_hold_ns);
	if (rc && rc != -EINVAL) {
		dev_err(&kp->spmi->dev, "Unable to parse 'row-hold-ns'\n");
		return rc;
	}

	rc = of_property_read_u32(kp->spmi->dev.of_node,
					"qcom,debounce-ms", &kp->debounce_ms);
	if (rc && rc != -EINVAL) {
		dev_err(&kp->spmi->dev, "Unable to parse 'debounce-ms'\n");
		return rc;
	}

	kp->wakeup = of_property_read_bool(kp->spmi->dev.of_node,
							"qcom,wakeup");

	kp->rep = !of_property_read_bool(kp->spmi->dev.of_node,
					"linux,keypad-no-autorepeat");

	map = of_get_property(kp->spmi->dev.of_node,
					"linux,keymap", &keymap_len);
	if (!map) {
		dev_err(&kp->spmi->dev, "Keymap not specified\n");
		return -EINVAL;
	}

	keymap_data = devm_kzalloc(&kp->spmi->dev,
					sizeof(*keymap_data), GFP_KERNEL);
	if (!keymap_data) {
		dev_err(&kp->spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	keymap_data->keymap_size = keymap_len / sizeof(u32);

	keymap = devm_kzalloc(&kp->spmi->dev,
		sizeof(uint32_t) * keymap_data->keymap_size, GFP_KERNEL);
	if (!keymap) {
		dev_err(&kp->spmi->dev, "could not allocate memory for keymap\n");
		return -ENOMEM;
	}

	for (i = 0; i < keymap_data->keymap_size; i++) {
		unsigned int key = be32_to_cpup(map + i);
		int keycode, row, col;

		row = (key >> 24) & 0xff;
		col = (key >> 16) & 0xff;
		keycode = key & 0xffff;
		keymap[i] = KEY(row, col, keycode);
	}
	keymap_data->keymap = keymap;
	kp->keymap_data = keymap_data;

	return 0;
}

static int qpnp_kp_probe(struct spmi_device *spmi)
{
	struct qpnp_kp *kp;
	struct resource *keypad_base;
	int rc = 0;

	kp = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_kp), GFP_KERNEL);
	if (!kp) {
		dev_err(&spmi->dev, "%s: Can't allocate qpnp_kp\n",
			__func__);
		return -ENOMEM;
	}

	kp->spmi = spmi;

	rc = qpnp_keypad_parse_dt(kp);
	if (rc < 0) {
		dev_err(&spmi->dev, "Error parsing device tree\n");
		return rc;
	}

	/* the #rows and #columns are compulsary */
	if (!kp->num_cols || !kp->num_rows ||
		kp->num_cols > QPNP_MAX_COLS ||
		kp->num_rows > QPNP_MAX_ROWS ||
		kp->num_cols < QPNP_MIN_COLS ||
		kp->num_rows < QPNP_MIN_ROWS) {
		dev_err(&spmi->dev, "invalid rows/cols input data\n");
		return -EINVAL;
	}

	if (!kp->keymap_data) {
		dev_err(&spmi->dev, "keymap not specified\n");
		return -EINVAL;
	}

	/* the below parameters are optional*/
	if (!kp->scan_delay_ms) {
		kp->scan_delay_ms = KEYP_DEFAULT_SCAN_DELAY;
	} else {
		if (kp->scan_delay_ms > MAX_SCAN_DELAY ||
			kp->scan_delay_ms < MIN_SCAN_DELAY) {
			dev_err(&spmi->dev,
				"invalid keypad scan time supplied\n");
			return -EINVAL;
		}
	}

	if (!kp->row_hold_ns) {
		kp->row_hold_ns = MIN_ROW_HOLD_DELAY;
	} else {
		if (kp->row_hold_ns > MAX_ROW_HOLD_DELAY ||
			kp->row_hold_ns < MIN_ROW_HOLD_DELAY) {
			dev_err(&spmi->dev,
				"invalid keypad row hold time supplied\n");
			return -EINVAL;
		}
	}

	if (!kp->debounce_ms) {
		kp->debounce_ms = KEYP_DEFAULT_DEBOUNCE;
	} else {
		if (kp->debounce_ms > MAX_DEBOUNCE_TIME ||
			kp->debounce_ms < MIN_DEBOUNCE_TIME ||
			(kp->debounce_ms % 5 != 0)) {
			dev_err(&spmi->dev,
				"invalid debounce time supplied\n");
			return -EINVAL;
		}
	}

	kp->input = input_allocate_device();
	if (!kp->input) {
		dev_err(&spmi->dev, "Can't allocate keypad input device\n");
		return -ENOMEM;
	}

	kp->key_sense_irq = spmi_get_irq_byname(spmi, NULL, "kp-sense");
	if (kp->key_sense_irq < 0) {
		dev_err(&spmi->dev, "Unable to get keypad sense irq\n");
		return kp->key_sense_irq;
	}

	kp->key_stuck_irq = spmi_get_irq_byname(spmi, NULL, "kp-stuck");
	if (kp->key_stuck_irq < 0) {
		dev_err(&spmi->dev, "Unable to get stuck irq\n");
		return kp->key_stuck_irq;
	}

	keypad_base = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!keypad_base) {
		dev_err(&spmi->dev, "Unable to get keypad base address\n");
		return -ENXIO;
	}
	kp->base = keypad_base->start;

	kp->input->name = "qpnp_keypad";
	kp->input->phys = "qpnp_keypad/input0";
	kp->input->id.version	= 0x0001;
	kp->input->id.product	= 0x0001;
	kp->input->id.vendor	= 0x0001;

	kp->input->evbit[0]	= BIT_MASK(EV_KEY);

	if (kp->rep)
		set_bit(EV_REP, kp->input->evbit);

	kp->input->keycode	= kp->keycodes;
	kp->input->keycodemax	= QPNP_MATRIX_MAX_SIZE;
	kp->input->keycodesize	= sizeof(kp->keycodes);
	kp->input->open		= qpnp_kp_open;
	kp->input->close	= qpnp_kp_close;

	matrix_keypad_build_keymap(kp->keymap_data, NULL,
					kp->num_rows, kp->num_cols,
					kp->keycodes, kp->input);

	input_set_capability(kp->input, EV_MSC, MSC_SCAN);
	input_set_drvdata(kp->input, kp);
	dev_set_drvdata(&spmi->dev, kp);

	/* initialize keypad state */
	memset(kp->keystate, 0xff, sizeof(kp->keystate));
	memset(kp->stuckstate, 0xff, sizeof(kp->stuckstate));

	rc = qpnp_kpd_init(kp);
	if (rc < 0) {
		dev_err(&spmi->dev, "unable to initialize keypad controller\n");
		return rc;
	}

	rc = input_register_device(kp->input);
	if (rc < 0) {
		dev_err(&spmi->dev, "unable to register keypad input device\n");
		return rc;
	}

	rc = devm_request_irq(&spmi->dev, kp->key_sense_irq, qpnp_kp_irq,
				 IRQF_TRIGGER_RISING, "qpnp-keypad-sense", kp);
	if (rc < 0) {
		dev_err(&spmi->dev, "failed to request keypad sense irq\n");
		return rc;
	}

	rc = devm_request_irq(&spmi->dev, kp->key_stuck_irq, qpnp_kp_stuck_irq,
				 IRQF_TRIGGER_RISING, "qpnp-keypad-stuck", kp);
	if (rc < 0) {
		dev_err(&spmi->dev, "failed to request keypad stuck irq\n");
		return rc;
	}

	device_init_wakeup(&spmi->dev, kp->wakeup);

	return rc;
}

static int qpnp_kp_remove(struct spmi_device *spmi)
{
	struct qpnp_kp *kp = dev_get_drvdata(&spmi->dev);

	device_init_wakeup(&spmi->dev, 0);
	input_unregister_device(kp->input);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int qpnp_kp_suspend(struct device *dev)
{
	struct qpnp_kp *kp = dev_get_drvdata(dev);
	struct input_dev *input_dev = kp->input;

	if (device_may_wakeup(dev)) {
		enable_irq_wake(kp->key_sense_irq);
	} else {
		mutex_lock(&input_dev->mutex);

		if (input_dev->users)
			qpnp_kp_disable(kp);

		mutex_unlock(&input_dev->mutex);
	}

	return 0;
}

static int qpnp_kp_resume(struct device *dev)
{
	struct qpnp_kp *kp = dev_get_drvdata(dev);
	struct input_dev *input_dev = kp->input;

	if (device_may_wakeup(dev)) {
		disable_irq_wake(kp->key_sense_irq);
	} else {
		mutex_lock(&input_dev->mutex);

		if (input_dev->users)
			qpnp_kp_enable(kp);

		mutex_unlock(&input_dev->mutex);
	}

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(qpnp_kp_pm_ops,
			 qpnp_kp_suspend, qpnp_kp_resume);

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-keypad",
	},
	{}
};

static struct spmi_driver qpnp_kp_driver = {
	.probe		= qpnp_kp_probe,
	.remove		= qpnp_kp_remove,
	.driver		= {
		.name = "qcom,qpnp-keypad",
		.of_match_table = spmi_match_table,
		.owner = THIS_MODULE,
		.pm = &qpnp_kp_pm_ops,
	},
};

static int __init qpnp_kp_init(void)
{
	return spmi_driver_register(&qpnp_kp_driver);
}
module_init(qpnp_kp_init);

static void __exit qpnp_kp_exit(void)
{
	spmi_driver_unregister(&qpnp_kp_driver);
}
module_exit(qpnp_kp_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QPNP keypad driver");

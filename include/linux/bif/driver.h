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

#ifndef _LINUX_BIF_DRIVER_H_
#define _LINUX_BIF_DRIVER_H_

#include <linux/device.h>
#include <linux/err.h>
#include <linux/types.h>
#include <linux/bif/consumer.h>

/**
 * struct bif_ctrl_dev - opaque handle used to identify a given BIF controller
 *			device
 */
struct bif_ctrl_dev;

/**
 * struct bif_ctrl_ops - BIF operations which may be implemented by BIF
 *				controller drivers
 * @bus_transaction:		Perform the specified BIF transaction which does
 *				not result in any slave response.
 * @bus_transaction_query:	Perform the specified BIF transaction which
 *				expects a BQ response in the case of slave
 *				positive acknowledgement.
 * @bus_transaction_read:	Perform the specified BIF transaction which
 *				expects an RD or TACK response from the selected
 *				slave.
 * @read_slave_registers:	Perform all BIF transactions necessary to read
 *				the specified set of contiguous registers from
 *				the previously selected slave.  This operation
 *				is used to optimize the common case of slave
 *				register reads since the a BIF controller driver
 *				can take advantage of BIF burst reads while the
 *				BIF core driver cannot due to the inherient
 *				tight timing requirements.
 * @write_slave_registers:	Perform all BIF transactions necessary to write
 *				the specified set of contiguous registers to
 *				the previously selected slave.  This operation
 *				is used to optimize the common case of slave
 *				register writes since the a BIF controller
 *				driver can remove redundant steps when
 *				performing several WD commands in a row.
 * @get_bus_period:		Return the tau_bif BIF bus clock period in
 *				nanoseconds.
 * @set_bus_period:		Set the tau_bif BIF bus clock period in
 *				nanoseconds.  If the exact period is not
 *				supported by the BIF controller hardware, then
 *				the next larger supported period should be used.
 * @get_battery_presence:	Return the current state of the battery pack.
 *				If a battery pack is present, then return >= 1.
 *				If a battery pack is not present, then return 0.
 *				If an error occurs during presence detection,
 *				then return errno.
 * @get_battery_rid:		Return the measured value of the Rid battery
 *				pack pull-down resistor in ohms.
 * @get_bus_state:		Return the current bus state as defined by one
 *				of the enum bif_bus_state values.
 * @set_bus_state:		Set the BIF bus state to the specified enum
 *				bif_bus_state value.
 *
 * The following operations must be defined by every BIF controller driver in
 * order to ensure baseline functionality:
 * bus_transaction, bus_transaction_query, get_bus_state, and set_bus_state.
 *
 * The BIF core driver is unaware of BIF transaction timing constraints.  A
 * given BIF controller driver must ensure that all timing constraints in the
 * MIPI-BIF specification are met as transactions are carried out.
 *
 * Conversion between 11-bit and 17-bit BIF words (i.e. the insertion of BCF_n,
 * parity bits, and the inversion bit) must be handled inside of the BIF
 * controller driver (either in software or hardware).  This guarantees maximum
 * performance if hardware support is available.
 *
 * The bus_transaction_read operation must return -ETIMEDOUT in the case of no
 * RD or TACK word received.  This allows the transaction query, TQ, command
 * to be used for slave selection verification.
 *
 * It is acceptable for the BIF bus state to be changed autonomously by a BIF
 * controller driver in response to low level bus actions without a call to
 * set_bus_state.  One example is the case of receiving a slave interrupt
 * while in interrupt state as this intrinsically causes the bus to enter the
 * active communication state.
 */
struct bif_ctrl_ops {
	int (*bus_transaction) (struct bif_ctrl_dev *bdev, int transaction,
					u8 data);
	int (*bus_transaction_query) (struct bif_ctrl_dev *bdev,
					int transaction, u8 data,
					bool *query_response);
	int (*bus_transaction_read) (struct bif_ctrl_dev *bdev,
					int transaction, u8 data,
					int *response);
	int (*read_slave_registers) (struct bif_ctrl_dev *bdev, u16 addr,
					u8 *data, int len);
	int (*write_slave_registers) (struct bif_ctrl_dev *bdev, u16 addr,
					const u8 *data, int len);
	int (*get_bus_period) (struct bif_ctrl_dev *bdev);
	int (*set_bus_period) (struct bif_ctrl_dev *bdev, int period_ns);
	int (*get_battery_presence) (struct bif_ctrl_dev *bdev);
	int (*get_battery_rid) (struct bif_ctrl_dev *bdev);
	int (*get_bus_state) (struct bif_ctrl_dev *bdev);
	int (*set_bus_state) (struct bif_ctrl_dev *bdev, int state);
};

/**
 * struct bif_ctrl_desc - BIF bus controller descriptor
 * @name:		Name used to identify the BIF controller
 * @ops:		BIF operations supported by the BIF controller
 * @bus_clock_min_ns:	Minimum tau_bif BIF bus clock period supported by the
 *			BIF controller
 * @bus_clock_max_ns:	Maximum tau_bif BIF bus clock period supported by the
 *			BIF controller
 *
 * Each BIF controller registered with the BIF core is described with a
 * structure of this type.
 */
struct bif_ctrl_desc {
	const char *name;
	struct bif_ctrl_ops *ops;
	int bus_clock_min_ns;
	int bus_clock_max_ns;
};

#ifdef CONFIG_BIF

struct bif_ctrl_dev *bif_ctrl_register(struct bif_ctrl_desc *bif_desc,
	struct device *dev, void *driver_data, struct device_node *of_node);

void bif_ctrl_unregister(struct bif_ctrl_dev *bdev);

void *bdev_get_drvdata(struct bif_ctrl_dev *bdev);

int bif_ctrl_notify_battery_changed(struct bif_ctrl_dev *bdev);
int bif_ctrl_notify_slave_irq(struct bif_ctrl_dev *bdev);

#else

static inline struct bif_ctrl_dev *bif_ctrl_register(
	struct bif_ctrl_desc *bif_desc, struct device *dev, void *driver_data,
	struct device_node *of_node)
{ return ERR_PTR(-EINVAL); }

static inline void bif_ctrl_unregister(struct bif_ctrl_dev *bdev) { }

static inline void *bdev_get_drvdata(struct bif_ctrl_dev *bdev) { return NULL; }

int bif_ctrl_notify_slave_irq(struct bif_ctrl_dev *bdev) { return -EINVAL; }

#endif

#endif

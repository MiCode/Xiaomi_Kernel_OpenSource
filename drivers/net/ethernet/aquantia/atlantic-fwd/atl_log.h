/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_LOG_H_
#define _ATL_LOG_H_

#ifdef __KERNEL__

#include <linux/device.h>

/* Logging conveniency macros.
 *
 * atl_dev_xxx are for low-level contexts and implicitly reference
 * struct atl_hw *hw;
 *
 * atl_nic_xxx are for high-level contexts and implicitly reference
 * struct atl_nic *nic;
 */
#define atl_dev_dbg(fmt, args...)			\
	dev_dbg(&hw->pdev->dev, fmt, ## args)
#define atl_dev_info(fmt, args...)			\
	dev_info(&hw->pdev->dev, fmt, ## args)
#define atl_dev_warn(fmt, args...)			\
	dev_warn(&hw->pdev->dev, fmt, ## args)
#define atl_dev_err(fmt, args...)			\
	dev_err(&hw->pdev->dev, fmt, ## args)

#define atl_nic_dbg(fmt, args...)		\
	dev_dbg(&nic->hw.pdev->dev, fmt, ## args)
#define atl_nic_info(fmt, args...)		\
	dev_info(&nic->hw.pdev->dev, fmt, ## args)
#define atl_nic_warn(fmt, args...)		\
	dev_warn(&nic->hw.pdev->dev, fmt, ## args)
#define atl_nic_err(fmt, args...)		\
	dev_err(&nic->hw.pdev->dev, fmt, ## args)

#define atl_dev_init_warn(fmt, args...)					\
do {									\
	if (hw)								\
		atl_dev_warn(fmt, ## args);				\
	else								\
		printk(KERN_WARNING "%s: " fmt, atl_driver_name, ##args); \
} while (0)

#define atl_dev_init_err(fmt, args...)					\
do {									\
	if (hw)								\
		atl_dev_err(fmt, ## args);				\
	else								\
		printk(KERN_ERR "%s: " fmt, atl_driver_name, ##args);	\
} while (0)

#else /* __KERNEL__ */

/* Logging is disabled in case of unit tests */
#define atl_dev_dbg(fmt, args...)
#define atl_dev_info(fmt, args...)
#define atl_dev_warn(fmt, args...)
#define atl_dev_err(fmt, args...)
#define atl_nic_dbg(fmt, args...)
#define atl_nic_info(fmt, args...)
#define atl_nic_warn(fmt, args...)
#define atl_nic_err(fmt, args...)
#define atl_dev_init_warn(fmt, args...)
#define atl_dev_init_err(fmt, args...)

#endif /* __KERNEL__ */

#endif /* _ATL_LOG_H_ */

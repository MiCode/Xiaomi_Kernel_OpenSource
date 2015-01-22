/*
 * mdm_ctrl.h
 *
 * Intel Mobile Communication modem boot driver
 *
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Contact: Faouaz Tenoutit <faouazx.tenoutit@intel.com>
 *          Frederic Berat <fredericx.berat@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 */
#ifndef _MDM_CTRL_H
#define _MDM_CTRL_H

#include <linux/ioctl.h>

/* Modem state */
enum {
	MDM_CTRL_STATE_UNKNOWN			= 0x0000,
	MDM_CTRL_STATE_OFF			= 0x0001,
	MDM_CTRL_STATE_COLD_BOOT		= 0x0002,
	MDM_CTRL_STATE_WARM_BOOT		= 0x0004,
	MDM_CTRL_STATE_COREDUMP			= 0x0008,
	MDM_CTRL_STATE_IPC_READY		= 0x0010,
	MDM_CTRL_STATE_FW_DOWNLOAD_READY	= 0x0020,
};

/* Backward compatibility with previous patches */
#define MDM_CTRL_STATE_NONE MDM_CTRL_STATE_UNKNOWN

/* Modem hanging up reasons */
enum {
	MDM_CTRL_NO_HU		= 0x00,
	MDM_CTRL_HU_RESET	= 0x01,
	MDM_CTRL_HU_COREDUMP	= 0x02,
};

enum {
	MDM_TIMER_FLASH_ENABLE,
	MDM_TIMER_FLASH_DISABLE,
	MDM_TIMER_DEFAULT
};

/* Supported Modem IDs*/
enum mdm_ctrl_mdm_type {
	MODEM_UNSUP,
	MODEM_2230,
	MODEM_6260,
	MODEM_6268,
	MODEM_6360,
	MODEM_7160,
	MODEM_7260,
	MODEM_7360
};

/* Type of modem board */
enum mdm_ctrl_board_type {
	BOARD_UNSUP,
	BOARD_AOB,
	BOARD_NGFF,
	BOARD_PCIE,
};

/* Type of power on control */
enum mdm_ctrl_pwr_on_type {
	POWER_ON_UNSUP,
	POWER_ON_PMIC_GPIO,
	POWER_ON_GPIO,
	POWER_ON_PMIC
};

/**
 * struct mdm_ctrl_cmd - Command parameters
 *
 * @timeout: the command timeout duration
 * @curr_state: the current modem state
 * @expected_state: the modem state to wait for
 */
struct mdm_ctrl_cmd {
	unsigned int param;
	unsigned int timeout;
};

/**
 * struct mdm_ctrl_cfg - MCD configuration
 *
 * @board board type
 * @type modem family type
 * @pwr_on_ctrl power on method
 * @usb_hub_ctrl usage of usb hub ctrl
 */
struct mdm_ctrl_cfg {
	enum mdm_ctrl_board_type board;
	enum mdm_ctrl_mdm_type type;
	enum mdm_ctrl_pwr_on_type pwr_on;
	unsigned int usb_hub;
};

#define MDM_CTRL_MAGIC	0x87 /* FIXME: Revisit */

/* IOCTL commands list */
#define MDM_CTRL_POWER_OFF		_IO(MDM_CTRL_MAGIC, 0)
#define MDM_CTRL_POWER_ON		_IO(MDM_CTRL_MAGIC, 1)
#define MDM_CTRL_WARM_RESET		_IO(MDM_CTRL_MAGIC, 2)
#define MDM_CTRL_COLD_RESET		_IO(MDM_CTRL_MAGIC, 3)
#define MDM_CTRL_SET_STATE		_IO(MDM_CTRL_MAGIC, 4)
#define MDM_CTRL_GET_STATE		_IO(MDM_CTRL_MAGIC, 5)
#define MDM_CTRL_RESERVED		_IO(MDM_CTRL_MAGIC, 6)
#define MDM_CTRL_FLASHING_WARM_RESET	_IO(MDM_CTRL_MAGIC, 7)
#define MDM_CTRL_GET_HANGUP_REASONS	_IO(MDM_CTRL_MAGIC, 8)
#define MDM_CTRL_CLEAR_HANGUP_REASONS	_IO(MDM_CTRL_MAGIC, 9)
#define MDM_CTRL_SET_POLLED_STATES	_IO(MDM_CTRL_MAGIC, 10)
#define MDM_CTRL_SET_CFG		_IO(MDM_CTRL_MAGIC, 11)

#endif /* _MDM_CTRL_H */

